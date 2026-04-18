#pragma once

/**
 * SignalRoute — Ingestion Validator
 *
 * Validates incoming LocationEvents against the schema rules:
 *   - Required fields present (device_id, lat, lon, timestamp_ms, seq)
 *   - Coordinate bounds: lat ∈ [-90, 90], lon ∈ [-180, 180]
 *   - Timestamp: not in the future (within skew tolerance), not too old
 *   - Sequence number: > 0
 *   - Batch size: ≤ max_batch_events
 *
 * Thread-safe: stateless validator, safe to call from multiple threads.
 */

#include "../common/config/config.h"
#include "../common/types/location_event.h"
#include "../common/types/result.h"
#include <vector>

namespace signalroute {

class Validator {
public:
    explicit Validator(const GatewayConfig& config);

    /**
     * Validate a single event.
     *
     * TODO: Implement validation checks:
     *   1. device_id must be non-empty
     *   2. lat ∈ [-90, 90]
     *   3. lon ∈ [-180, 180]
     *   4. timestamp_ms must be within [now - 24h, now + skew_tolerance]
     *   5. seq > 0
     *   6. accuracy_m >= 0
     *
     * @return Result<void> — ok() if valid, err(reason) if invalid
     */
    Result<void, std::string> validate(const LocationEvent& event) const;

    /**
     * Validate a batch of events.
     * Also checks batch-level constraints (batch size ≤ max_batch_events).
     *
     * @return Per-event validation results
     */
    std::vector<Result<void, std::string>> validate_batch(
        const std::vector<LocationEvent>& events) const;

private:
    GatewayConfig config_;
};

} // namespace signalroute
