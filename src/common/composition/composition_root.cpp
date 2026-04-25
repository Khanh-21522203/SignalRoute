#include "composition_root.h"

namespace signalroute {

CompositionRoot::CompositionRoot(EventBus& bus) : bus_(bus) {}

void CompositionRoot::wire_location_pipeline_observers() {
    subscriptions_.push_back(bus_.subscribe<events::LocationAccepted>(
        [this](const events::LocationAccepted& event) {
            bus_.publish(events::StateWriteRequested{event.event});
            bus_.publish(events::TripHistoryWriteRequested{event.event});
        }));

    subscriptions_.push_back(bus_.subscribe<events::StateWriteSucceeded>(
        [this](const events::StateWriteSucceeded& event) {
            bus_.publish(events::GeofenceEvaluationRequested{
                event.state.device_id,
                event.previous_h3_cell,
                event.state.h3_cell,
                event.state.lat,
                event.state.lon,
                event.state.updated_at});
        }));
}

void CompositionRoot::clear() {
    subscriptions_.clear();
}

std::size_t CompositionRoot::subscription_count() const {
    return subscriptions_.size();
}

} // namespace signalroute
