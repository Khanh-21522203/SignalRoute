#pragma once

/**
 * SignalRoute — MatchContext
 *
 * Framework services exposed to strategy plugins.
 * Provides reservation, search expansion, and time management.
 */

#include <string>
#include <vector>
#include <cstdint>

namespace signalroute {

struct MatchCandidate; // forward

class MatchContext {
public:
    virtual ~MatchContext() = default;

    /**
     * Attempt to atomically reserve an agent for this request.
     * Uses Redis SET NX with TTL for atomic reservation.
     *
     * @param agent_id Agent to reserve
     * @return true if reservation succeeded (agent was available)
     */
    virtual bool reserve(const std::string& agent_id) = 0;

    /**
     * Release a previously reserved agent.
     * Only releases if this request holds the reservation.
     */
    virtual void release(const std::string& agent_id) = 0;

    /**
     * Expand search: fetch agents in a wider radius.
     * Used when initial candidates are insufficient.
     */
    virtual std::vector<MatchCandidate> nearby(
        double lat, double lon, double radius_m, int limit) = 0;

    /**
     * Remaining time before the request deadline.
     * Strategies should check this to avoid exceeding the TTL.
     *
     * @return Milliseconds remaining, or 0 if expired
     */
    virtual int64_t time_remaining_ms() const = 0;

    /// The unique request ID (for reservation tracking).
    virtual const std::string& request_id() const = 0;
};

} // namespace signalroute
