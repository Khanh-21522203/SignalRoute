#pragma once

/**
 * SignalRoute — Redis Client
 *
 * Connection-pooled Redis client providing typed operations for:
 *   - Device state (Hash: dev:{id})
 *   - H3 cell index (Set: h3:{cell})
 *   - Fence state (Hash: fence:{dev}:{fence})
 *   - Agent reservation (for matching)
 *
 * Lua scripts are used for atomic CAS operations (sequence guard)
 * and combined state+index updates.
 *
 * Dependencies: hiredis in real Redis builds.
 */

#include "../config/config.h"
#include "../types/device_state.h"
#include "../types/geofence_types.h"

#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include <mutex>
#include <memory>
#include <utility>
#include <unordered_map>
#include <unordered_set>

namespace signalroute {

class RedisClient {
public:
    explicit RedisClient(const RedisConfig& config);
    ~RedisClient();

    // Disallow copy, allow move
    RedisClient(const RedisClient&) = delete;
    RedisClient& operator=(const RedisClient&) = delete;
    RedisClient(RedisClient&&) noexcept;
    RedisClient& operator=(RedisClient&&) noexcept;

    // ── Connection ──

    /// Test connectivity to Redis.
    bool ping();

    // ── Device State (Hash: {prefix}:dev:{device_id}) ──

    /**
     * Atomically update device state using CAS on sequence number.
     * Only writes if incoming seq > stored seq (or device is new).
     * Also updates H3 cell index if the cell changed.
     *
     * Uses a Lua script for atomicity (single EVALSHA call).
     *
     * @param device_id Device identifier
     * @param state New device state to write
     * @param ttl_s TTL in seconds for the key (reset on every write)
     * @return true if accepted (seq was newer), false if rejected
     *
     * TODO: Implement Lua CAS script from docs/storage/schema.md
     */
    bool update_device_state_cas(const std::string& device_id,
                                 const DeviceState& state,
                                 int ttl_s);

    /**
     * Read the latest state for a device.
     * @return DeviceState if found, std::nullopt if not found or expired.
     *
     * TODO: Implement using HGETALL {prefix}:dev:{device_id}
     */
    std::optional<DeviceState> get_device_state(const std::string& device_id);

    /**
     * Batch-read device states using Redis pipeline.
     * Issues all HGETALL commands in a single pipeline for efficiency.
     *
     * TODO: Implement pipelined HGETALL
     */
    std::vector<std::optional<DeviceState>> get_device_states_batch(
        const std::vector<std::string>& device_ids);

    // ── H3 Cell Index (Set: {prefix}:h3:{cell_id}) ──

    /**
     * Add a device to an H3 cell set.
     * TODO: Implement using SADD {prefix}:h3:{cell_id} {device_id}
     */
    void add_device_to_cell(int64_t cell_id, const std::string& device_id);

    /**
     * Remove a device from an H3 cell set.
     * TODO: Implement using SREM {prefix}:h3:{cell_id} {device_id}
     */
    void remove_device_from_cell(int64_t cell_id, const std::string& device_id);

    /**
     * Get all devices in a single H3 cell.
     * TODO: Implement using SMEMBERS {prefix}:h3:{cell_id}
     */
    std::vector<std::string> get_devices_in_cell(int64_t cell_id);

    /**
     * Get all devices across multiple H3 cells (pipelined SMEMBERS).
     * Used by NearbyHandler for k-ring queries.
     *
     * TODO: Implement using pipelined SMEMBERS, return union of all sets.
     */
    std::vector<std::string> get_devices_in_cells(const std::vector<int64_t>& cells);

    /**
     * Remove H3 cell members that no longer have a device state record.
     * Fallback helper for H3 cleanup worker tests until Redis keyspace
     * notifications and production expiry handling are integrated.
     *
     * @return {removed_devices, touched_cells}
     */
    std::pair<std::size_t, std::size_t> remove_stale_cell_members();

    // ── Fence State (Hash: {prefix}:fence:{device_id}:{fence_id}) ──

    /**
     * Set the fence state for a (device, fence) pair.
     * TODO: Implement using HMSET {prefix}:fence:{dev}:{fence} state entered_at exited_at
     */
    void set_fence_state(const std::string& device_id,
                         const std::string& fence_id,
                         FenceState state, int64_t timestamp_ms);

    /**
     * Get the fence state for a (device, fence) pair.
     */
    std::optional<FenceState> get_fence_state(const std::string& device_id,
                                              const std::string& fence_id);

    /**
     * Get full fence state metadata for a (device, fence) pair.
     */
    std::optional<FenceStateRecord> get_fence_state_record(const std::string& device_id,
                                                           const std::string& fence_id);

    /**
     * List all stored fence states that match the requested state.
     * Used by dwell checking fallback until Redis scanning is implemented.
     */
    std::vector<FenceStateRecord> list_fence_states(FenceState state);

    // ── Agent Reservation (Matching) ──

    /**
     * Attempt to atomically reserve an agent for a match request.
     * Uses SET NX EX for atomic reservation with TTL.
     *
     * @param agent_id Agent to reserve
     * @param request_id Request holding the reservation
     * @param ttl_ms Reservation TTL in milliseconds
     * @return true if reserved, false if already reserved by another request
     *
     * TODO: Implement using SET {prefix}:reservation:{agent_id} {request_id} NX PX {ttl_ms}
     */
    bool try_reserve_agent(const std::string& agent_id,
                           const std::string& request_id, int ttl_ms);

    /**
     * Release a reservation. Only releases if this request holds it.
     * TODO: Implement using Lua compare-and-delete
     */
    void release_agent(const std::string& agent_id,
                       const std::string& request_id);

    /**
     * Check whether an agent has a non-expired reservation.
     * Fallback helper for matching tests; production Redis uses key existence.
     */
    bool is_agent_reserved(const std::string& agent_id) const;

    /**
     * Return the request currently holding a non-expired reservation.
     */
    std::optional<std::string> get_agent_reservation_holder(const std::string& agent_id) const;

private:
    struct Impl;

    struct ReservationRecord {
        std::string request_id;
        int64_t expires_at_ms = 0;
    };

    RedisConfig config_;
    std::unique_ptr<Impl> impl_;

    mutable std::mutex mu_;
    std::unordered_map<std::string, DeviceState> device_states_;
    std::unordered_map<int64_t, std::unordered_set<std::string>> cell_devices_;
    std::unordered_map<std::string, FenceStateRecord> fence_states_;
    std::unordered_map<std::string, ReservationRecord> reservations_;

    // TODO: Pre-loaded Lua script SHAs
    // std::string cas_update_sha_;
    // std::string cas_delete_sha_;
};

} // namespace signalroute
