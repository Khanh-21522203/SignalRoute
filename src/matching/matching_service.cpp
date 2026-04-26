#include "matching_service.h"

#include "match_context.h"
#include "nearest_strategy.h"
#include "reservation_manager.h"
#include "strategy_interface.h"
#include "strategy_registry.h"
#include "../common/clients/redis_client.h"
#include "../common/events/all_events.h"
#include "../common/events/event_bus.h"
#include "../common/spatial/h3_index.h"
#include "../query/nearby_handler.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <cmath>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace signalroute {

namespace {

int64_t epoch_now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

class TrackingMatchContext final : public MatchContext {
public:
    explicit TrackingMatchContext(MatchContext& inner) : inner_(inner) {}

    bool reserve(const std::string& agent_id) override {
        if (!inner_.reserve(agent_id)) {
            return false;
        }
        if (std::find(reserved_.begin(), reserved_.end(), agent_id) == reserved_.end()) {
            reserved_.push_back(agent_id);
        }
        return true;
    }

    void release(const std::string& agent_id) override {
        inner_.release(agent_id);
        reserved_.erase(std::remove(reserved_.begin(), reserved_.end(), agent_id), reserved_.end());
    }

    std::vector<MatchCandidate> nearby(double lat, double lon, double radius_m, int limit) override {
        return inner_.nearby(lat, lon, radius_m, limit);
    }

    int64_t time_remaining_ms() const override {
        return inner_.time_remaining_ms();
    }

    const std::string& request_id() const override {
        return inner_.request_id();
    }

    void release_all() {
        auto reserved = reserved_;
        for (const auto& agent_id : reserved) {
            release(agent_id);
        }
    }

    void release_unassigned(const std::vector<std::string>& assigned_agent_ids) {
        const std::unordered_set<std::string> assigned(
            assigned_agent_ids.begin(), assigned_agent_ids.end());
        auto reserved = reserved_;
        for (const auto& agent_id : reserved) {
            if (assigned.count(agent_id) == 0) {
                release(agent_id);
            }
        }
    }

    bool owns_all(const std::vector<std::string>& agent_ids) const {
        std::unordered_set<std::string> reserved(reserved_.begin(), reserved_.end());
        for (const auto& agent_id : agent_ids) {
            if (agent_id.empty() || reserved.count(agent_id) == 0) {
                return false;
            }
        }
        return true;
    }

private:
    MatchContext& inner_;
    std::vector<std::string> reserved_;
};

MatchResult failed_result(const MatchRequest& request, std::string reason) {
    MatchResult result;
    result.request_id = request.request_id;
    result.status = MatchStatus::FAILED;
    result.reason = std::move(reason);
    return result;
}

std::vector<MatchCandidate> to_candidates(const NearbyResult& nearby) {
    std::vector<MatchCandidate> candidates;
    candidates.reserve(nearby.devices.size());
    for (const auto& [state, distance] : nearby.devices) {
        MatchCandidate candidate;
        candidate.agent_id = state.device_id;
        candidate.lat = state.lat;
        candidate.lon = state.lon;
        candidate.distance_m = distance;
        candidate.available = true;
        candidates.push_back(candidate);
    }
    return candidates;
}

class ServiceMatchContext final : public MatchContext {
public:
    ServiceMatchContext(const MatchRequest& request,
                        ReservationManager& reservations,
                        NearbyHandler& nearby_handler,
                        int64_t /*started_ms*/)
        : request_(request)
        , reservations_(reservations)
        , nearby_handler_(nearby_handler)
    {}

    bool reserve(const std::string& agent_id) override {
        return reservations_.reserve(agent_id, request_.request_id);
    }

    void release(const std::string& agent_id) override {
        reservations_.release(agent_id, request_.request_id);
    }

    std::vector<MatchCandidate> nearby(double lat, double lon, double radius_m, int limit) override {
        return to_candidates(nearby_handler_.handle(lat, lon, radius_m, limit, 0));
    }

    int64_t time_remaining_ms() const override {
        if (request_.deadline_ms <= 0) {
            return config_default_remaining_ms_;
        }
        const int64_t remaining = request_.deadline_ms - epoch_now_ms();
        return remaining > 0 ? remaining : 0;
    }

    const std::string& request_id() const override {
        return request_.request_id;
    }

private:
    const MatchRequest& request_;
    ReservationManager& reservations_;
    NearbyHandler& nearby_handler_;
    static constexpr int64_t config_default_remaining_ms_ = 60000;
};

} // namespace

MatchingService::MatchingService() = default;
MatchingService::~MatchingService() { if (running_) stop(); }

void MatchingService::start(const Config& config) {
    start_with_bus(config, nullptr);
}

void MatchingService::start(const Config& config, EventBus& event_bus) {
    start_with_bus(config, &event_bus);
}

void MatchingService::start_with_bus(const Config& config, EventBus* external_bus) {
    if (running_) {
        return;
    }

    std::cout << "[MatchingService] Starting...\n";
    config_ = config;
    if (external_bus == nullptr) {
        owned_bus_ = std::make_unique<EventBus>();
        event_bus_ = owned_bus_.get();
    } else {
        owned_bus_.reset();
        event_bus_ = external_bus;
    }

    register_builtin_strategies();
    strategy_ = StrategyRegistry::instance().create(config.matching.strategy_name);
    strategy_->initialize(config);
    strategy_name_ = config.matching.strategy_name;
    redis_ = std::make_unique<RedisClient>(config.redis);
    h3_ = std::make_unique<H3Index>(config.spatial.h3_resolution);
    nearby_handler_ = std::make_unique<NearbyHandler>(*redis_, *h3_, config.spatial);
    reservations_ = std::make_unique<ReservationManager>(*redis_, config.matching.request_ttl_ms);
    running_ = true;
    // Kafka/protobuf request consumption and result publication are integrated later.
    std::cout << "[MatchingService] Started.\n";
}

void MatchingService::stop() {
    std::cout << "[MatchingService] Stopping...\n";
    running_ = false;
    reservations_.reset();
    nearby_handler_.reset();
    h3_.reset();
    redis_.reset();
    strategy_.reset();
    std::cout << "[MatchingService] Stopped.\n";
}

bool MatchingService::is_healthy() const { return running_; }

const std::string& MatchingService::strategy_name() const {
    return strategy_name_;
}

MatchResult MatchingService::match_once(const MatchRequest& request,
                                        const std::vector<MatchCandidate>& candidates,
                                        MatchContext& context) {
    const int64_t started_ms = epoch_now_ms();
    if (!running_ || !strategy_) {
        return failed_result(request, "matching service is not running");
    }
    if (request.request_id.empty()) {
        return failed_result(request, "request_id is required");
    }
    if (request.deadline_ms > 0 && started_ms >= request.deadline_ms) {
        MatchResult result;
        result.request_id = request.request_id;
        result.status = MatchStatus::EXPIRED;
        result.reason = "request deadline expired";
        return result;
    }

    TrackingMatchContext tracking(context);
    try {
        auto assigned = strategy_->match(request, candidates, tracking);
        if (request.max_agents > 0 && static_cast<int>(assigned.size()) > request.max_agents) {
            assigned.resize(static_cast<size_t>(request.max_agents));
        }
        if (assigned.empty()) {
            tracking.release_all();
            return failed_result(request, "no agents matched");
        }
        if (!tracking.owns_all(assigned)) {
            tracking.release_all();
            return failed_result(request, "strategy returned unreserved agent");
        }

        tracking.release_unassigned(assigned);

        MatchResult result;
        result.request_id = request.request_id;
        result.status = MatchStatus::MATCHED;
        result.assigned_agent_ids = std::move(assigned);
        result.latency_ms = epoch_now_ms() - started_ms;
        return result;
    } catch (const std::exception& ex) {
        tracking.release_all();
        return failed_result(request, ex.what());
    } catch (...) {
        tracking.release_all();
        return failed_result(request, "matching strategy failed");
    }
}

MatchResult MatchingService::handle_request(MatchRequest request) {
    const int64_t started_ms = epoch_now_ms();
    if (!running_ || !nearby_handler_ || !reservations_) {
        return failed_result(request, "matching service is not running");
    }
    if (request.strategy.empty()) {
        request.strategy = strategy_name_;
    }
    if (request.deadline_ms == 0 && config_.matching.request_ttl_ms > 0) {
        request.deadline_ms = started_ms + config_.matching.request_ttl_ms;
    }
    if (request.max_agents <= 0) {
        request.max_agents = 1;
    }

    if (event_bus_) {
        event_bus_->publish(events::MatchRequestReceived{
            request.request_id,
            request.requester_id,
            request.lat,
            request.lon,
            request.radius_m,
            request.max_agents,
            request.deadline_ms,
            request.strategy});
    }

    if (request.deadline_ms > 0 && started_ms >= request.deadline_ms) {
        MatchResult result;
        result.request_id = request.request_id;
        result.status = MatchStatus::EXPIRED;
        result.reason = "request deadline expired";
        if (event_bus_) {
            event_bus_->publish(events::MatchExpired{request.request_id});
        }
        return result;
    }

    if (request.request_id.empty() || !std::isfinite(request.lat) || !std::isfinite(request.lon) ||
        !std::isfinite(request.radius_m) || request.radius_m <= 0.0) {
        auto result = failed_result(request, "invalid match request");
        if (event_bus_) {
            event_bus_->publish(events::MatchFailed{request.request_id, result.reason});
        }
        return result;
    }

    auto candidates = to_candidates(
        nearby_handler_->handle(
            request.lat,
            request.lon,
            request.radius_m,
            request.max_agents * 4,
            0));

    ServiceMatchContext context(request, *reservations_, *nearby_handler_, started_ms);
    auto result = match_once(request, candidates, context);
    result.latency_ms = epoch_now_ms() - started_ms;

    if (event_bus_) {
        if (result.status == MatchStatus::MATCHED) {
            event_bus_->publish(events::MatchCompleted{
                result.request_id,
                result.assigned_agent_ids,
                result.latency_ms});
        } else if (result.status == MatchStatus::EXPIRED) {
            event_bus_->publish(events::MatchExpired{result.request_id});
        } else {
            event_bus_->publish(events::MatchFailed{result.request_id, result.reason});
        }
    }

    return result;
}

bool MatchingService::seed_agent_for_test(DeviceState state) {
    if (!running_ || !redis_ || !h3_) {
        return false;
    }
    if (state.device_id.empty()) {
        return false;
    }
    if (state.h3_cell == 0) {
        state.h3_cell = h3_->lat_lng_to_cell(state.lat, state.lon);
    }
    return redis_->update_device_state_cas(state.device_id, state, config_.redis.device_ttl_s);
}

bool MatchingService::reserve_agent_for_test(
    const std::string& agent_id,
    const std::string& request_id) {
    return reservations_ && reservations_->reserve(agent_id, request_id);
}

bool MatchingService::is_agent_reserved_for_test(const std::string& agent_id) const {
    return reservations_ && reservations_->is_reserved(agent_id);
}

} // namespace signalroute
