#pragma once

#include "../common/events/event_bus.h"

#include <cstddef>
#include <vector>

namespace signalroute {

class Evaluator;

class GeofenceEventHandlers {
public:
    GeofenceEventHandlers(EventBus& bus, Evaluator& evaluator);

    void wire();
    void clear();

    [[nodiscard]] std::size_t subscription_count() const;

private:
    EventBus& bus_;
    Evaluator& evaluator_;
    std::vector<EventBus::Subscription> subscriptions_;
};

} // namespace signalroute
