#include "common/events/all_events.h"
#include "common/events/event_bus.h"
#include "common/metrics/metrics.h"
#include "workers/metrics_reporter.h"

#include <cassert>
#include <iostream>
#include <string>

void test_metrics_reporter_exports_text_and_publishes_recovered_event() {
    auto& metrics = signalroute::Metrics::instance();
    metrics.reset_for_test();
    metrics.inc_events_accepted(2);

    signalroute::EventBus bus;
    int recovered = 0;
    auto sub = bus.subscribe<signalroute::events::DependencyRecovered>(
        [&](const signalroute::events::DependencyRecovered& event) {
            if (event.dependency == "metrics") {
                ++recovered;
            }
        });

    signalroute::ObservabilityConfig config;
    signalroute::MetricsReporter reporter(config, bus);
    const auto first = reporter.run_once();
    const auto second = reporter.run_once();

    assert(first.export_count == 1);
    assert(second.export_count == 2);
    assert(first.text.find("events_accepted_total 2") != std::string::npos);
    assert(recovered == 2);
}

int main() {
    std::cout << "test_metrics_reporter:\n";
    test_metrics_reporter_exports_text_and_publishes_recovered_event();
    std::cout << "All metrics reporter tests passed.\n";
    return 0;
}
