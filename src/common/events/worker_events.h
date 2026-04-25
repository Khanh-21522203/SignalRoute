#pragma once

#include <cstddef>
#include <string>

namespace signalroute::events {

struct H3CleanupCompleted {
    std::size_t removed_devices = 0;
    std::size_t touched_cells = 0;
};

struct DLQReplaySucceeded {
    std::size_t replayed_messages = 0;
};

struct DLQReplayFailed {
    std::string reason;
    std::size_t failed_messages = 0;
};

struct DependencyRecovered {
    std::string dependency;
};

struct DependencyUnavailable {
    std::string dependency;
    std::string reason;
};

} // namespace signalroute::events
