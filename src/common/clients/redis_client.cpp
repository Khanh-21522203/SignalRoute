#include "redis_client.h"
#include <stdexcept>
#include <iostream>
#include <unordered_set>
#include <utility>
#include <chrono>

// TODO: #include <sw/redis++/redis++.h>

namespace signalroute {

namespace {

int64_t monotonic_now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

bool reservation_expired_at(int64_t expires_at_ms, int64_t now_ms) {
    return expires_at_ms <= now_ms;
}

} // namespace

RedisClient::RedisClient(const RedisConfig& config) : config_(config) {
    // TODO: Initialize connection pool
    //   sw::redis::ConnectionOptions opts;
    //   opts.host = /* parse from config.addrs */;
    //   opts.port = /* parse from config.addrs */;
    //   opts.connect_timeout = std::chrono::milliseconds(config.connect_timeout_ms);
    //   opts.socket_timeout = std::chrono::milliseconds(config.read_timeout_ms);
    //
    //   sw::redis::ConnectionPoolOptions pool_opts;
    //   pool_opts.size = config.pool_size;
    //
    //   auto redis = sw::redis::Redis(opts, pool_opts);
    //
    // TODO: Pre-load Lua scripts via SCRIPT LOAD
    //   cas_update_sha_ = redis.script_load(CAS_UPDATE_LUA);

    std::cerr << "[RedisClient] WARNING: Redis client not yet implemented.\n";
}

RedisClient::~RedisClient() = default;
RedisClient::RedisClient(RedisClient&& other) noexcept {
    std::lock_guard lock(other.mu_);
    config_ = std::move(other.config_);
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
    device_states_ = std::move(other.device_states_);
    cell_devices_ = std::move(other.cell_devices_);
    fence_states_ = std::move(other.fence_states_);
    reservations_ = std::move(other.reservations_);
    return *this;
}

bool RedisClient::ping() {
    return true;
}

bool RedisClient::update_device_state_cas(
    const std::string& device_id,
    const DeviceState& state,
    int /*ttl_s*/)
{
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
}

std::optional<DeviceState> RedisClient::get_device_state(const std::string& device_id) {
    std::lock_guard lock(mu_);
    auto it = device_states_.find(device_id);
    if (it == device_states_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<std::optional<DeviceState>> RedisClient::get_device_states_batch(
    const std::vector<std::string>& device_ids)
{
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
}

void RedisClient::add_device_to_cell(int64_t cell_id, const std::string& device_id) {
    std::lock_guard lock(mu_);
    cell_devices_[cell_id].insert(device_id);
}

void RedisClient::remove_device_from_cell(int64_t cell_id, const std::string& device_id) {
    std::lock_guard lock(mu_);
    auto it = cell_devices_.find(cell_id);
    if (it == cell_devices_.end()) {
        return;
    }
    it->second.erase(device_id);
    if (it->second.empty()) {
        cell_devices_.erase(it);
    }
}

std::vector<std::string> RedisClient::get_devices_in_cell(int64_t cell_id) {
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
}

std::vector<std::string> RedisClient::get_devices_in_cells(const std::vector<int64_t>& cells) {
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
}

std::pair<std::size_t, std::size_t> RedisClient::remove_stale_cell_members() {
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
}

void RedisClient::set_fence_state(const std::string& device_id,
                                   const std::string& fence_id,
                                   FenceState state, int64_t timestamp_ms) {
    std::lock_guard lock(mu_);
    const auto key = device_id + ":" + fence_id;
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
}

std::optional<FenceState> RedisClient::get_fence_state(const std::string& device_id,
                                                        const std::string& fence_id) {
    std::lock_guard lock(mu_);
    auto it = fence_states_.find(device_id + ":" + fence_id);
    if (it == fence_states_.end()) {
        return std::nullopt;
    }
    return it->second.state;
}

std::optional<FenceStateRecord> RedisClient::get_fence_state_record(
    const std::string& device_id,
    const std::string& fence_id) {
    std::lock_guard lock(mu_);
    auto it = fence_states_.find(device_id + ":" + fence_id);
    if (it == fence_states_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<FenceStateRecord> RedisClient::list_fence_states(FenceState state) {
    std::lock_guard lock(mu_);
    std::vector<FenceStateRecord> records;
    for (const auto& [_, record] : fence_states_) {
        if (record.state == state) {
            records.push_back(record);
        }
    }
    return records;
}

bool RedisClient::try_reserve_agent(const std::string& agent_id,
                                     const std::string& request_id, int ttl_ms) {
    if (agent_id.empty() || request_id.empty() || ttl_ms <= 0) {
        return false;
    }

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
}

void RedisClient::release_agent(const std::string& agent_id,
                                 const std::string& request_id) {
    std::lock_guard lock(mu_);
    auto it = reservations_.find(agent_id);
    if (it != reservations_.end() && it->second.request_id == request_id) {
        reservations_.erase(it);
    }
}

bool RedisClient::is_agent_reserved(const std::string& agent_id) const {
    std::lock_guard lock(mu_);
    auto it = reservations_.find(agent_id);
    if (it == reservations_.end()) {
        return false;
    }
    return !reservation_expired_at(it->second.expires_at_ms, monotonic_now_ms());
}

std::optional<std::string> RedisClient::get_agent_reservation_holder(
    const std::string& agent_id) const {
    std::lock_guard lock(mu_);
    auto it = reservations_.find(agent_id);
    if (it == reservations_.end() ||
        reservation_expired_at(it->second.expires_at_ms, monotonic_now_ms())) {
        return std::nullopt;
    }
    return it->second.request_id;
}

} // namespace signalroute
