#pragma once

#include "../events/event_bus.h"
#include "../events/all_events.h"

#include <cstddef>
#include <vector>

namespace signalroute {

class CompositionRoot {
public:
    explicit CompositionRoot(EventBus& bus);
    ~CompositionRoot() = default;

    CompositionRoot(const CompositionRoot&) = delete;
    CompositionRoot& operator=(const CompositionRoot&) = delete;
    CompositionRoot(CompositionRoot&&) noexcept = default;
    CompositionRoot& operator=(CompositionRoot&&) noexcept = default;

    void wire_location_pipeline_observers();
    void clear();

    [[nodiscard]] std::size_t subscription_count() const;

private:
    EventBus& bus_;
    std::vector<EventBus::Subscription> subscriptions_;
};

} // namespace signalroute
