#pragma once

/**
 * SignalRoute — IMatchStrategy Interface
 *
 * THE PLUGIN CONTRACT for the Matching Server framework.
 *
 * Domain engineers implement this interface to create custom matching
 * algorithms. The framework handles all infrastructure concerns:
 * Kafka I/O, agent reservation, timeout management, and metrics.
 *
 * Example strategies:
 *   - "nearest": Closest available agent (greedy)
 *   - "nearest_k": Top-K nearest, pick by rating/ETA
 *   - "batch": Batch optimization over a request window
 */

#include "matching_types.h"

#include <memory>
#include <string>
#include <vector>

namespace signalroute {

// Forward declarations. Protobuf conversion belongs at the transport boundary.
class MatchContext;       // framework services
class Config;

class IMatchStrategy {
public:
    virtual ~IMatchStrategy() = default;

    /**
     * Called once at startup with the strategy's config subtree.
     * Use this to load model weights, configure parameters, etc.
     */
    virtual void initialize(const Config& config) = 0;

    /**
     * Core matching function. Called by the framework per MatchRequest.
     *
     * The strategy receives:
     *   - request: The incoming match request with requester location
     *   - candidates: Pre-filtered nearby available agents
     *   - context: Framework services (reserve, release, nearby, time_remaining)
     *
     * The strategy may:
     *   - Call context.reserve(agent_id) to atomically claim an agent
     *   - Call context.release(agent_id) if reservation should be rolled back
     *   - Call context.nearby() to expand search radius
     *   - Check context.time_remaining_ms() to avoid deadline
     *
     * @return List of matched agent IDs, or empty on failure
     */
    virtual std::vector<std::string> match(
        const MatchRequest& request,
        const std::vector<MatchCandidate>& candidates,
        MatchContext& context) = 0;

    /// Human-readable name for logging and metrics.
    virtual std::string name() const = 0;
};

} // namespace signalroute
