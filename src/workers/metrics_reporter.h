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
#include <cstddef>
#include <string>

namespace signalroute {

class EventBus;

struct MetricsReport {
    std::size_t export_count = 0;
    std::string text;
};

class MetricsReporter {
public:
    explicit MetricsReporter(const ObservabilityConfig& config);
    MetricsReporter(const ObservabilityConfig& config, EventBus& event_bus);

    MetricsReport run_once();

    /// Run the reporter. Blocks until should_stop is set.
    void run(std::atomic<bool>& should_stop);

private:
    ObservabilityConfig config_;
    EventBus* event_bus_ = nullptr;
    std::size_t export_count_ = 0;
};

} // namespace signalroute
