#include "geofence_event_handlers.h"

#include "evaluator.h"
#include "../common/events/all_events.h"

namespace signalroute {

GeofenceEventHandlers::GeofenceEventHandlers(EventBus& bus, Evaluator& evaluator)
    : bus_(bus), evaluator_(evaluator) {}

void GeofenceEventHandlers::wire() {
    subscriptions_.push_back(bus_.subscribe<events::GeofenceEvaluationRequested>(
        [this](const events::GeofenceEvaluationRequested& event) {
            evaluator_.evaluate(
                event.device_id,
                event.old_h3_cell,
                event.new_h3_cell,
                event.lat,
                event.lon,
                event.timestamp_ms);
        }));
}

void GeofenceEventHandlers::clear() {
    subscriptions_.clear();
}

std::size_t GeofenceEventHandlers::subscription_count() const {
    return subscriptions_.size();
}

} // namespace signalroute
