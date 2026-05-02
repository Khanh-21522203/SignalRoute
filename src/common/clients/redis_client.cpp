#include "redis_client.h"

#if SIGNALROUTE_HAS_REDIS
#include <hiredis/hiredis.h>
#endif

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#if SIGNALROUTE_HAS_REDIS
#include <sys/time.h>
#endif

namespace signalroute {

namespace {

int64_t monotonic_now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

bool reservation_expired_at(int64_t expires_at_ms, int64_t now_ms) {
    return expires_at_ms <= now_ms;
}

std::string fence_key(const std::string& device_id, const std::string& fence_id) {
    return device_id + ":" + fence_id;
}

#if SIGNALROUTE_HAS_REDIS

struct RedisContextDeleter {
    void operator()(redisContext* context) const {
        if (context) {
            redisFree(context);
        }
    }
};

struct RedisReplyDeleter {
    void operator()(redisReply* reply) const {
        if (reply) {
            freeReplyObject(reply);
        }
    }
};

using RedisReplyPtr = std::unique_ptr<redisReply, RedisReplyDeleter>;

std::pair<std::string, int> parse_redis_addr(const std::string& addrs) {
    const auto comma = addrs.find(',');
    const std::string first = addrs.substr(0, comma);
    const auto colon = first.rfind(':');
    if (colon == std::string::npos) {
        return {first.empty() ? "localhost" : first, 6379};
    }
    return {first.substr(0, colon), std::stoi(first.substr(colon + 1))};
}

std::string key_prefix(const RedisConfig& config, const std::string& suffix) {
    return config.key_prefix.empty() ? suffix : config.key_prefix + ":" + suffix;
}

std::string device_key(const RedisConfig& config, const std::string& device_id) {
    return key_prefix(config, "dev:" + device_id);
}

std::string cell_key(const RedisConfig& config, int64_t cell_id) {
    return key_prefix(config, "h3:" + std::to_string(cell_id));
}

std::string fence_redis_key(
    const RedisConfig& config,
    const std::string& device_id,
    const std::string& fence_id) {
    return key_prefix(config, "fence:" + device_id + ":" + fence_id);
}

std::string reservation_key(const RedisConfig& config, const std::string& agent_id) {
    return key_prefix(config, "reservation:" + agent_id);
}

std::string cell_scan_pattern(const RedisConfig& config) {
    return key_prefix(config, "h3:*");
}

std::string fence_scan_pattern(const RedisConfig& config) {
    return key_prefix(config, "fence:*");
}

std::string to_string(double value) {
    return std::to_string(value);
}

std::string to_string(float value) {
    return std::to_string(value);
}

std::string to_string(int64_t value) {
    return std::to_string(value);
}

std::string to_string(uint64_t value) {
    return std::to_string(value);
}

std::string to_string(long long value) {
    return std::to_string(value);
}

std::string to_string(int value) {
    return std::to_string(value);
}

timeval timeout_from_ms(int timeout_ms) {
    const int normalized = std::max(1, timeout_ms);
    timeval timeout{};
    timeout.tv_sec = normalized / 1000;
    timeout.tv_usec = (normalized % 1000) * 1000;
    return timeout;
}

RedisReplyPtr redis_command(redisContext* context, const std::vector<std::string>& args) {
    if (!context || args.empty()) {
        return nullptr;
    }

    std::vector<const char*> argv;
    std::vector<std::size_t> argvlen;
    argv.reserve(args.size());
    argvlen.reserve(args.size());
    for (const auto& arg : args) {
        argv.push_back(arg.data());
        argvlen.push_back(arg.size());
    }

    return RedisReplyPtr(static_cast<redisReply*>(
        redisCommandArgv(
            context,
            static_cast<int>(args.size()),
            argv.data(),
            argvlen.data())));
}

RedisReplyPtr redis_command(redisContext* context, std::initializer_list<std::string> args) {
    return redis_command(context, std::vector<std::string>(args));
}

std::string reply_string(const redisReply* reply) {
    if (!reply || !reply->str) {
        return {};
    }
    return {reply->str, reply->len};
}

bool reply_status_equals(const redisReply* reply, const std::string& expected) {
    return reply &&
           reply->type == REDIS_REPLY_STATUS &&
           reply_string(reply) == expected;
}

std::unordered_map<std::string, std::string> hash_from_reply(const redisReply* reply) {
    std::unordered_map<std::string, std::string> fields;
    if (!reply || reply->type != REDIS_REPLY_ARRAY) {
        return fields;
    }

    for (std::size_t i = 0; i + 1 < reply->elements; i += 2) {
        fields.emplace(
            reply_string(reply->element[i]),
            reply_string(reply->element[i + 1]));
    }
    return fields;
}

std::vector<std::string> string_array_from_reply(const redisReply* reply) {
    std::vector<std::string> values;
    if (!reply || reply->type != REDIS_REPLY_ARRAY) {
        return values;
    }

    values.reserve(reply->elements);
    for (std::size_t i = 0; i < reply->elements; ++i) {
        values.push_back(reply_string(reply->element[i]));
    }
    return values;
}

DeviceState device_state_from_hash(const std::unordered_map<std::string, std::string>& fields) {
    auto get = [&](const std::string& field) -> std::string {
        auto it = fields.find(field);
        return it == fields.end() ? std::string{} : it->second;
    };

    DeviceState state;
    state.device_id = get("device_id");
    state.lat = std::stod(get("lat"));
    state.lon = std::stod(get("lon"));
    state.altitude_m = static_cast<float>(std::stof(get("altitude_m")));
    state.accuracy_m = static_cast<float>(std::stof(get("accuracy_m")));
    state.speed_ms = static_cast<float>(std::stof(get("speed_ms")));
    state.heading_deg = static_cast<float>(std::stof(get("heading_deg")));
    state.h3_cell = std::stoll(get("h3_cell"));
    state.seq = static_cast<uint64_t>(std::stoull(get("seq")));
    state.updated_at = std::stoll(get("updated_at"));
    return state;
}

FenceStateRecord fence_state_from_hash(
    const std::string& device_id,
    const std::string& fence_id,
    const std::unordered_map<std::string, std::string>& fields) {
    auto get = [&](const std::string& field) -> std::string {
        auto it = fields.find(field);
        return it == fields.end() ? std::string{} : it->second;
    };

    FenceStateRecord record;
    record.device_id = device_id;
    record.fence_id = fence_id;
    record.state = fence_state_from_string(get("state"));
    record.entered_at_ms = std::stoll(get("entered_at_ms"));
    record.updated_at_ms = std::stoll(get("updated_at_ms"));
    return record;
}

std::pair<std::string, std::string> parse_fence_ids(const RedisConfig& config, const std::string& key) {
    const std::string prefix = key_prefix(config, "fence:");
    if (key.rfind(prefix, 0) != 0) {
        return {"", ""};
    }
    const auto rest = key.substr(prefix.size());
    const auto sep = rest.find(':');
    if (sep == std::string::npos) {
        return {"", ""};
    }
    return {rest.substr(0, sep), rest.substr(sep + 1)};
}

#endif

} // namespace

struct RedisClient::Impl {
#if SIGNALROUTE_HAS_REDIS
    std::unique_ptr<redisContext, RedisContextDeleter> redis;
#endif
};

RedisClient::RedisClient(const RedisConfig& config) : config_(config), impl_(std::make_unique<Impl>()) {
#if SIGNALROUTE_HAS_REDIS
    const auto [host, port] = parse_redis_addr(config_.addrs);

    auto connect_timeout = timeout_from_ms(config_.connect_timeout_ms);
    impl_->redis.reset(redisConnectWithTimeout(host.c_str(), port, connect_timeout));
    if (impl_->redis && impl_->redis->err == 0) {
        auto read_timeout = timeout_from_ms(config_.read_timeout_ms);
        redisSetTimeout(impl_->redis.get(), read_timeout);
    }
#else
    std::cerr << "[RedisClient] WARNING: using in-memory Redis fallback.\n";
#endif
}

RedisClient::~RedisClient() = default;

RedisClient::RedisClient(RedisClient&& other) noexcept {
    std::lock_guard lock(other.mu_);
    config_ = std::move(other.config_);
    impl_ = std::move(other.impl_);
    device_states_ = std::move(other.device_states_);
    cell_devices_ = std::move(other.cell_devices_);
    fence_states_ = std::move(other.fence_states_);
    reservations_ = std::move(other.reservations_);
}

RedisClient& RedisClient::operator=(RedisClient&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    std::scoped_lock lock(mu_, other.mu_);
    config_ = std::move(other.config_);
    impl_ = std::move(other.impl_);
    device_states_ = std::move(other.device_states_);
    cell_devices_ = std::move(other.cell_devices_);
    fence_states_ = std::move(other.fence_states_);
    reservations_ = std::move(other.reservations_);
    return *this;
}

bool RedisClient::ping() {
#if SIGNALROUTE_HAS_REDIS
    try {
        if (!impl_ || !impl_->redis || impl_->redis->err != 0) {
            return false;
        }
        auto reply = redis_command(impl_->redis.get(), {"PING"});
        return reply_status_equals(reply.get(), "PONG");
    } catch (const std::exception&) {
        return false;
    }
#else
    return true;
#endif
}

bool RedisClient::update_device_state_cas(
    const std::string& device_id,
    const DeviceState& state,
    int ttl_s)
{
#if SIGNALROUTE_HAS_REDIS
    if (device_id.empty()) {
        return false;
    }

    const auto key = device_key(config_, device_id);
    const auto current = get_device_state(device_id);
    if (current && state.seq <= current->seq) {
        return false;
    }

    if (current && current->h3_cell != state.h3_cell) {
        redis_command(impl_->redis.get(), {"SREM", cell_key(config_, current->h3_cell), device_id});
    }

    DeviceState stored = state;
    stored.device_id = device_id;
    redis_command(impl_->redis.get(), {
        "HSET", key,
        "device_id", stored.device_id,
        "lat", to_string(stored.lat),
        "lon", to_string(stored.lon),
        "altitude_m", to_string(stored.altitude_m),
        "accuracy_m", to_string(stored.accuracy_m),
        "speed_ms", to_string(stored.speed_ms),
        "heading_deg", to_string(stored.heading_deg),
        "h3_cell", to_string(stored.h3_cell),
        "seq", to_string(stored.seq),
        "updated_at", to_string(stored.updated_at),
    });
    if (ttl_s > 0) {
        redis_command(impl_->redis.get(), {"EXPIRE", key, to_string(ttl_s)});
    }
    redis_command(impl_->redis.get(), {"SADD", cell_key(config_, stored.h3_cell), device_id});
    return true;
#else
    std::lock_guard lock(mu_);
    auto it = device_states_.find(device_id);
    if (it != device_states_.end() && state.seq <= it->second.seq) {
        return false;
    }

    if (it != device_states_.end() && it->second.h3_cell != state.h3_cell) {
        auto cell_it = cell_devices_.find(it->second.h3_cell);
        if (cell_it != cell_devices_.end()) {
            cell_it->second.erase(device_id);
            if (cell_it->second.empty()) {
                cell_devices_.erase(cell_it);
            }
        }
    }

    DeviceState stored = state;
    stored.device_id = device_id;
    device_states_[device_id] = stored;
    cell_devices_[stored.h3_cell].insert(device_id);
    return true;
#endif
}

std::optional<DeviceState> RedisClient::get_device_state(const std::string& device_id) {
#if SIGNALROUTE_HAS_REDIS
    auto reply = redis_command(impl_->redis.get(), {"HGETALL", device_key(config_, device_id)});
    const auto fields = hash_from_reply(reply.get());
    if (fields.empty()) {
        return std::nullopt;
    }
    return device_state_from_hash(fields);
#else
    std::lock_guard lock(mu_);
    auto it = device_states_.find(device_id);
    if (it == device_states_.end()) {
        return std::nullopt;
    }
    return it->second;
#endif
}

std::vector<std::optional<DeviceState>> RedisClient::get_device_states_batch(
    const std::vector<std::string>& device_ids)
{
#if SIGNALROUTE_HAS_REDIS
    std::vector<std::optional<DeviceState>> states;
    states.reserve(device_ids.size());
    for (const auto& device_id : device_ids) {
        states.push_back(get_device_state(device_id));
    }
    return states;
#else
    std::lock_guard lock(mu_);
    std::vector<std::optional<DeviceState>> states;
    states.reserve(device_ids.size());
    for (const auto& device_id : device_ids) {
        auto it = device_states_.find(device_id);
        if (it == device_states_.end()) {
            states.push_back(std::nullopt);
        } else {
            states.push_back(it->second);
        }
    }
    return states;
#endif
}

void RedisClient::add_device_to_cell(int64_t cell_id, const std::string& device_id) {
#if SIGNALROUTE_HAS_REDIS
    redis_command(impl_->redis.get(), {"SADD", cell_key(config_, cell_id), device_id});
#else
    std::lock_guard lock(mu_);
    cell_devices_[cell_id].insert(device_id);
#endif
}

void RedisClient::remove_device_from_cell(int64_t cell_id, const std::string& device_id) {
#if SIGNALROUTE_HAS_REDIS
    redis_command(impl_->redis.get(), {"SREM", cell_key(config_, cell_id), device_id});
#else
    std::lock_guard lock(mu_);
    auto it = cell_devices_.find(cell_id);
    if (it == cell_devices_.end()) {
        return;
    }
    it->second.erase(device_id);
    if (it->second.empty()) {
        cell_devices_.erase(it);
    }
#endif
}

std::vector<std::string> RedisClient::get_devices_in_cell(int64_t cell_id) {
#if SIGNALROUTE_HAS_REDIS
    auto reply = redis_command(impl_->redis.get(), {"SMEMBERS", cell_key(config_, cell_id)});
    return string_array_from_reply(reply.get());
#else
    std::lock_guard lock(mu_);
    std::vector<std::string> devices;
    auto it = cell_devices_.find(cell_id);
    if (it == cell_devices_.end()) {
        return devices;
    }
    devices.reserve(it->second.size());
    for (const auto& device_id : it->second) {
        devices.push_back(device_id);
    }
    return devices;
#endif
}

std::vector<std::string> RedisClient::get_devices_in_cells(const std::vector<int64_t>& cells) {
#if SIGNALROUTE_HAS_REDIS
    std::unordered_set<std::string> unique;
    for (int64_t cell : cells) {
        auto devices = get_devices_in_cell(cell);
        unique.insert(devices.begin(), devices.end());
    }
    return {unique.begin(), unique.end()};
#else
    std::lock_guard lock(mu_);
    std::unordered_set<std::string> unique;
    for (int64_t cell : cells) {
        auto it = cell_devices_.find(cell);
        if (it == cell_devices_.end()) {
            continue;
        }
        unique.insert(it->second.begin(), it->second.end());
    }
    return {unique.begin(), unique.end()};
#endif
}

std::pair<std::size_t, std::size_t> RedisClient::remove_stale_cell_members() {
#if SIGNALROUTE_HAS_REDIS
    std::size_t removed = 0;
    std::size_t touched = 0;
    long long cursor = 0;
    do {
        auto scan_reply = redis_command(impl_->redis.get(), {
            "SCAN", to_string(cursor), "MATCH", cell_scan_pattern(config_), "COUNT", "100"});
        if (!scan_reply || scan_reply->type != REDIS_REPLY_ARRAY || scan_reply->elements != 2) {
            break;
        }
        cursor = std::stoll(reply_string(scan_reply->element[0]));
        const auto keys = string_array_from_reply(scan_reply->element[1]);
        for (const auto& key : keys) {
            auto members_reply = redis_command(impl_->redis.get(), {"SMEMBERS", key});
            const auto devices = string_array_from_reply(members_reply.get());
            std::size_t removed_from_cell = 0;
            for (const auto& device_id : devices) {
                if (!get_device_state(device_id)) {
                    auto removed_reply = redis_command(impl_->redis.get(), {"SREM", key, device_id});
                    if (removed_reply && removed_reply->type == REDIS_REPLY_INTEGER) {
                        removed_from_cell += static_cast<std::size_t>(removed_reply->integer);
                    }
                }
            }
            if (removed_from_cell > 0) {
                removed += removed_from_cell;
                ++touched;
            }
        }
    } while (cursor != 0);
    return {removed, touched};
#else
    std::lock_guard lock(mu_);
    std::size_t removed = 0;
    std::size_t touched = 0;

    for (auto cell_it = cell_devices_.begin(); cell_it != cell_devices_.end();) {
        auto& devices = cell_it->second;
        const auto before = devices.size();
        for (auto device_it = devices.begin(); device_it != devices.end();) {
            if (device_states_.find(*device_it) == device_states_.end()) {
                device_it = devices.erase(device_it);
            } else {
                ++device_it;
            }
        }

        const auto removed_from_cell = before - devices.size();
        if (removed_from_cell > 0) {
            removed += removed_from_cell;
            ++touched;
        }
        if (devices.empty()) {
            cell_it = cell_devices_.erase(cell_it);
        } else {
            ++cell_it;
        }
    }

    return {removed, touched};
#endif
}

void RedisClient::set_fence_state(const std::string& device_id,
                                   const std::string& fence_id,
                                   FenceState state, int64_t timestamp_ms) {
#if SIGNALROUTE_HAS_REDIS
    int64_t entered_at_ms = 0;
    if (state == FenceState::INSIDE) {
        entered_at_ms = timestamp_ms;
    } else if (state == FenceState::DWELL) {
        auto previous = get_fence_state_record(device_id, fence_id);
        entered_at_ms = previous && previous->entered_at_ms > 0
            ? previous->entered_at_ms
            : timestamp_ms;
    }

    redis_command(impl_->redis.get(), {
        "HSET", fence_redis_key(config_, device_id, fence_id),
        "state", fence_state_to_string(state),
        "entered_at_ms", to_string(entered_at_ms),
        "updated_at_ms", to_string(timestamp_ms),
    });
#else
    std::lock_guard lock(mu_);
    const auto key = fence_key(device_id, fence_id);
    auto previous = fence_states_.find(key);

    FenceStateRecord record;
    record.device_id = device_id;
    record.fence_id = fence_id;
    record.state = state;
    record.updated_at_ms = timestamp_ms;

    if (state == FenceState::INSIDE) {
        record.entered_at_ms = timestamp_ms;
    } else if (state == FenceState::DWELL) {
        if (previous != fence_states_.end() && previous->second.entered_at_ms > 0) {
            record.entered_at_ms = previous->second.entered_at_ms;
        } else {
            record.entered_at_ms = timestamp_ms;
        }
    }

    fence_states_[key] = record;
#endif
}

std::optional<FenceState> RedisClient::get_fence_state(const std::string& device_id,
                                                        const std::string& fence_id) {
#if SIGNALROUTE_HAS_REDIS
    auto record = get_fence_state_record(device_id, fence_id);
    if (!record) {
        return std::nullopt;
    }
    return record->state;
#else
    std::lock_guard lock(mu_);
    auto it = fence_states_.find(fence_key(device_id, fence_id));
    if (it == fence_states_.end()) {
        return std::nullopt;
    }
    return it->second.state;
#endif
}

std::optional<FenceStateRecord> RedisClient::get_fence_state_record(
    const std::string& device_id,
    const std::string& fence_id) {
#if SIGNALROUTE_HAS_REDIS
    auto reply = redis_command(
        impl_->redis.get(),
        {"HGETALL", fence_redis_key(config_, device_id, fence_id)});
    const auto fields = hash_from_reply(reply.get());
    if (fields.empty()) {
        return std::nullopt;
    }
    return fence_state_from_hash(device_id, fence_id, fields);
#else
    std::lock_guard lock(mu_);
    auto it = fence_states_.find(fence_key(device_id, fence_id));
    if (it == fence_states_.end()) {
        return std::nullopt;
    }
    return it->second;
#endif
}

std::vector<FenceStateRecord> RedisClient::list_fence_states(FenceState state) {
#if SIGNALROUTE_HAS_REDIS
    std::vector<FenceStateRecord> records;
    long long cursor = 0;
    do {
        auto scan_reply = redis_command(impl_->redis.get(), {
            "SCAN", to_string(cursor), "MATCH", fence_scan_pattern(config_), "COUNT", "100"});
        if (!scan_reply || scan_reply->type != REDIS_REPLY_ARRAY || scan_reply->elements != 2) {
            break;
        }
        cursor = std::stoll(reply_string(scan_reply->element[0]));
        const auto keys = string_array_from_reply(scan_reply->element[1]);
        for (const auto& key : keys) {
            const auto [device_id, fence_id] = parse_fence_ids(config_, key);
            if (device_id.empty() || fence_id.empty()) {
                continue;
            }
            auto record = get_fence_state_record(device_id, fence_id);
            if (record && record->state == state) {
                records.push_back(*record);
            }
        }
    } while (cursor != 0);
    return records;
#else
    std::lock_guard lock(mu_);
    std::vector<FenceStateRecord> records;
    for (const auto& [_, record] : fence_states_) {
        if (record.state == state) {
            records.push_back(record);
        }
    }
    return records;
#endif
}

bool RedisClient::try_reserve_agent(const std::string& agent_id,
                                     const std::string& request_id, int ttl_ms) {
    if (agent_id.empty() || request_id.empty() || ttl_ms <= 0) {
        return false;
    }

#if SIGNALROUTE_HAS_REDIS
    auto reply = redis_command(impl_->redis.get(), {
        "SET", reservation_key(config_, agent_id), request_id, "PX", to_string(ttl_ms), "NX"});
    return reply_status_equals(reply.get(), "OK");
#else
    std::lock_guard lock(mu_);
    const int64_t now_ms = monotonic_now_ms();
    auto it = reservations_.find(agent_id);
    if (it != reservations_.end()) {
        if (!reservation_expired_at(it->second.expires_at_ms, now_ms)) {
            return false;
        }
        reservations_.erase(it);
    }

    reservations_.emplace(agent_id, ReservationRecord{
        request_id,
        now_ms + static_cast<int64_t>(ttl_ms)
    });
    return true;
#endif
}

void RedisClient::release_agent(const std::string& agent_id,
                                 const std::string& request_id) {
#if SIGNALROUTE_HAS_REDIS
    auto holder = get_agent_reservation_holder(agent_id);
    if (holder && *holder == request_id) {
        redis_command(impl_->redis.get(), {"DEL", reservation_key(config_, agent_id)});
    }
#else
    std::lock_guard lock(mu_);
    auto it = reservations_.find(agent_id);
    if (it != reservations_.end() && it->second.request_id == request_id) {
        reservations_.erase(it);
    }
#endif
}

bool RedisClient::is_agent_reserved(const std::string& agent_id) const {
#if SIGNALROUTE_HAS_REDIS
    return get_agent_reservation_holder(agent_id).has_value();
#else
    std::lock_guard lock(mu_);
    auto it = reservations_.find(agent_id);
    if (it == reservations_.end()) {
        return false;
    }
    return !reservation_expired_at(it->second.expires_at_ms, monotonic_now_ms());
#endif
}

std::optional<std::string> RedisClient::get_agent_reservation_holder(
    const std::string& agent_id) const {
#if SIGNALROUTE_HAS_REDIS
    auto reply = redis_command(impl_->redis.get(), {"GET", reservation_key(config_, agent_id)});
    if (!reply || reply->type == REDIS_REPLY_NIL) {
        return std::nullopt;
    }
    return reply_string(reply.get());
#else
    std::lock_guard lock(mu_);
    auto it = reservations_.find(agent_id);
    if (it == reservations_.end() ||
        reservation_expired_at(it->second.expires_at_ms, monotonic_now_ms())) {
        return std::nullopt;
    }
    return it->second.request_id;
#endif
}

} // namespace signalroute
