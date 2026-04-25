#include "redis_client.h"
#include <stdexcept>
#include <iostream>
#include <unordered_set>
#include <utility>

// TODO: #include <sw/redis++/redis++.h>

namespace signalroute {

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

void RedisClient::set_fence_state(const std::string& device_id,
                                   const std::string& fence_id,
                                   FenceState state, int64_t /*timestamp_ms*/) {
    std::lock_guard lock(mu_);
    fence_states_[device_id + ":" + fence_id] = state;
}

std::optional<FenceState> RedisClient::get_fence_state(const std::string& device_id,
                                                        const std::string& fence_id) {
    std::lock_guard lock(mu_);
    auto it = fence_states_.find(device_id + ":" + fence_id);
    if (it == fence_states_.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool RedisClient::try_reserve_agent(const std::string& agent_id,
                                     const std::string& request_id, int /*ttl_ms*/) {
    std::lock_guard lock(mu_);
    auto [_, inserted] = reservations_.emplace(agent_id, request_id);
    return inserted;
}

void RedisClient::release_agent(const std::string& agent_id,
                                 const std::string& request_id) {
    std::lock_guard lock(mu_);
    auto it = reservations_.find(agent_id);
    if (it != reservations_.end() && it->second == request_id) {
        reservations_.erase(it);
    }
}

} // namespace signalroute
