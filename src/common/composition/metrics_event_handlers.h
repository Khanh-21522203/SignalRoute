#pragma once

#include "../events/event_bus.h"

#include <cstddef>
#include <vector>

namespace signalroute {

class MetricsEventHandlers {
public:
    explicit MetricsEventHandlers(EventBus& bus);

    void wire();
    void clear();

    [[nodiscard]] std::size_t subscription_count() const;

private:
    EventBus& bus_;
    std::vector<EventBus::Subscription> subscriptions_;
};

} // namespace signalroute
