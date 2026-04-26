#include "metrics_reporter.h"
#include "../common/events/all_events.h"
#include "../common/events/event_bus.h"
#include <thread>
#include <chrono>

namespace signalroute {

MetricsReporter::MetricsReporter(const ObservabilityConfig& config)
    : config_(config) {}

MetricsReporter::MetricsReporter(const ObservabilityConfig& config, EventBus& event_bus)
    : config_(config), event_bus_(&event_bus) {}

MetricsReport MetricsReporter::run_once() {
    ++export_count_;
    MetricsReport report;
    report.export_count = export_count_;
    report.text = Metrics::instance().export_text();
    if (event_bus_) {
        event_bus_->publish(events::DependencyRecovered{"metrics"});
    }
    return report;
}

void MetricsReporter::run(std::atomic<bool>& should_stop) {
    // TODO: Start Prometheus HTTP exposer and periodically collect system metrics
    while (!should_stop.load()) {
        (void)run_once();
        std::this_thread::sleep_for(std::chrono::seconds(15));
    }
}

} // namespace signalroute
