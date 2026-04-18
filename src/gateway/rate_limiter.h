#pragma once

/**
 * SignalRoute — Per-Device Rate Limiter
 *
 * Sliding window rate limiter to prevent device flood attacks.
 * Each device_id has an independent rate budget.
 *
 * Implementation: token bucket or sliding window counter stored in a
 * concurrent hash map with periodic cleanup of stale entries.
 *
 * Thread-safe: uses sharded locks for concurrent access.
 */

#include <string>
#include <cstdint>

namespace signalroute {

class RateLimiter {
public:
    /**
     * @param max_rps_per_device Maximum requests per second per device.
     */
    explicit RateLimiter(int max_rps_per_device);

    /**
     * Check if a request from this device should be allowed.
     *
     * @param device_id Device identifier
     * @return true if allowed, false if rate-limited
     *
     * TODO: Implement sliding window counter or token bucket
     *       Key requirements:
     *         - O(1) per call
     *         - Thread-safe (multiple gRPC threads call concurrently)
     *         - Periodic cleanup of stale entries to bound memory
     */
    bool allow(const std::string& device_id);

    /**
     * Get the current rate for a device (for metrics/debugging).
     *
     * @return Current rate in requests per second, or 0 if unknown
     */
    double current_rate(const std::string& device_id) const;

    /// Number of tracked devices.
    size_t tracked_devices() const;

private:
    int max_rps_;

    // TODO: Add concurrent hash map for per-device state
    // e.g., tbb::concurrent_hash_map<std::string, WindowState>
    // or std::unordered_map with sharded mutexes
};

} // namespace signalroute
