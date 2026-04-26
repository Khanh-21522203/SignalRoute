#include "metrics_event_handlers.h"

#include "../events/all_events.h"
#include "../metrics/metrics.h"

namespace signalroute {

MetricsEventHandlers::MetricsEventHandlers(EventBus& bus) : bus_(bus) {}

void MetricsEventHandlers::wire() {
    subscriptions_.push_back(bus_.subscribe<events::IngestBatchReceived>(
        [](const events::IngestBatchReceived& event) {
            Metrics::instance().inc_ingest_received(static_cast<int64_t>(event.events.size()));
        }));

    subscriptions_.push_back(bus_.subscribe<events::IngestBatchRejected>(
        [](const events::IngestBatchRejected& event) {
            Metrics::instance().inc_ingest_rejected(event.reason, event.rejected_count);
        }));

    subscriptions_.push_back(bus_.subscribe<events::IngestEventPublished>(
        [](const events::IngestEventPublished&) {
            Metrics::instance().inc_ingest_queued();
        }));

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

    subscriptions_.push_back(bus_.subscribe<events::MatchRequestReceived>(
        [](const events::MatchRequestReceived&) {
            Metrics::instance().inc_match_request();
        }));

    subscriptions_.push_back(bus_.subscribe<events::MatchCompleted>(
        [](const events::MatchCompleted& event) {
            Metrics::instance().inc_match_result("completed");
            Metrics::instance().observe_match_latency(static_cast<double>(event.latency_ms));
        }));

    subscriptions_.push_back(bus_.subscribe<events::MatchFailed>(
        [](const events::MatchFailed&) {
            Metrics::instance().inc_match_result("failed");
        }));

    subscriptions_.push_back(bus_.subscribe<events::MatchExpired>(
        [](const events::MatchExpired&) {
            Metrics::instance().inc_match_result("expired");
        }));
}

void MetricsEventHandlers::clear() {
    subscriptions_.clear();
}

std::size_t MetricsEventHandlers::subscription_count() const {
    return subscriptions_.size();
}

} // namespace signalroute
