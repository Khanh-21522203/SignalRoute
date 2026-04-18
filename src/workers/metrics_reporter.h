#pragma once

/**
 * SignalRoute — Metrics Reporter
 *
 * Background worker that periodically exports Prometheus metrics.
 * Also collects system-level gauges (pool utilization, lag, etc.).
 */

#include "../common/metrics/metrics.h"
#include "../common/config/config.h"
#include <atomic>

namespace signalroute {

class MetricsReporter {
public:
    explicit MetricsReporter(const ObservabilityConfig& config);

    /// Run the reporter. Blocks until should_stop is set.
    void run(std::atomic<bool>& should_stop);

private:
    ObservabilityConfig config_;
};

} // namespace signalroute
