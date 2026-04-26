#include "processor_event_handlers.h"

#include "history_writer.h"
#include "state_writer.h"
#include "../common/events/all_events.h"

namespace signalroute {

ProcessorEventHandlers::ProcessorEventHandlers(
    EventBus& bus,
    StateWriter& state_writer,
    HistoryWriter& history_writer)
    : bus_(bus)
    , state_writer_(state_writer)
    , history_writer_(history_writer)
{}

void ProcessorEventHandlers::wire() {
    subscriptions_.push_back(bus_.subscribe<events::StateWriteRequested>(
        [this](const events::StateWriteRequested& event) {
            auto outcome = state_writer_.write_with_result(event.event);
            if (!outcome.accepted) {
                bus_.publish(events::StateWriteRejected{event.event, "state write rejected"});
                return;
            }
            bus_.publish(events::StateWriteSucceeded{
                event.event,
                outcome.state,
                outcome.previous_h3_cell});
        }));

    subscriptions_.push_back(bus_.subscribe<events::TripHistoryWriteRequested>(
        [this](const events::TripHistoryWriteRequested& event) {
            history_writer_.buffer(event.event);
            bus_.publish(events::TripHistoryWritten{event.event});
            if (history_writer_.should_flush()) {
                history_writer_.flush();
            }
        }));
}

void ProcessorEventHandlers::clear() {
    subscriptions_.clear();
}

std::size_t ProcessorEventHandlers::subscription_count() const {
    return subscriptions_.size();
}

} // namespace signalroute
