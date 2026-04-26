#include "metrics_event_handlers.h"

#include "../events/all_events.h"
#include "../metrics/metrics.h"

namespace signalroute {

MetricsEventHandlers::MetricsEventHandlers(EventBus& bus) : bus_(bus) {}

void MetricsEventHandlers::wire() {
    subscriptions_.push_back(bus_.subscribe<events::LocationAccepted>(
        [](const events::LocationAccepted&) {
            Metrics::instance().inc_events_accepted();
        }));

    subscriptions_.push_back(bus_.subscribe<events::LocationDuplicateRejected>(
        [](const events::LocationDuplicateRejected&) {
            Metrics::instance().inc_dedup_hit();
        }));

    subscriptions_.push_back(bus_.subscribe<events::LocationStaleRejected>(
        [](const events::LocationStaleRejected&) {
            Metrics::instance().inc_seq_guard_reject();
        }));

    subscriptions_.push_back(bus_.subscribe<events::StateWriteRejected>(
        [](const events::StateWriteRejected&) {
            Metrics::instance().inc_truly_stale();
        }));

    subscriptions_.push_back(bus_.subscribe<events::GeofenceEntered>(
        [](const events::GeofenceEntered&) {
            Metrics::instance().inc_geofence_event("ENTER");
        }));

    subscriptions_.push_back(bus_.subscribe<events::GeofenceExited>(
        [](const events::GeofenceExited&) {
            Metrics::instance().inc_geofence_event("EXIT");
        }));

    subscriptions_.push_back(bus_.subscribe<events::GeofenceDwellDetected>(
        [](const events::GeofenceDwellDetected&) {
            Metrics::instance().inc_geofence_event("DWELL");
        }));
}

void MetricsEventHandlers::clear() {
    subscriptions_.clear();
}

std::size_t MetricsEventHandlers::subscription_count() const {
    return subscriptions_.size();
}

} // namespace signalroute
