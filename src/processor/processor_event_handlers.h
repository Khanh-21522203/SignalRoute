#pragma once

#include "../common/events/event_bus.h"

#include <cstddef>
#include <vector>

namespace signalroute {

class HistoryWriter;
class StateWriter;

class ProcessorEventHandlers {
public:
    ProcessorEventHandlers(EventBus& bus, StateWriter& state_writer, HistoryWriter& history_writer);

    void wire();
    void clear();

    [[nodiscard]] std::size_t subscription_count() const;

private:
    EventBus& bus_;
    StateWriter& state_writer_;
    HistoryWriter& history_writer_;
    std::vector<EventBus::Subscription> subscriptions_;
};

} // namespace signalroute
