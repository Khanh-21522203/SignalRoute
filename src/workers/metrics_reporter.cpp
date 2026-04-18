#include "metrics_reporter.h"
#include <thread>
#include <chrono>

namespace signalroute {

MetricsReporter::MetricsReporter(const ObservabilityConfig& config)
    : config_(config) {}

void MetricsReporter::run(std::atomic<bool>& should_stop) {
    // TODO: Start Prometheus HTTP exposer and periodically collect system metrics
    while (!should_stop.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(15));
    }
}

} // namespace signalroute
