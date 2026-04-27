#pragma once

#include <string>

namespace signalroute {

enum class ServiceLifecycleState {
    Stopped,
    Starting,
    Ready,
    Draining,
    Failed,
};

struct ServiceHealthSnapshot {
    ServiceLifecycleState state = ServiceLifecycleState::Stopped;
    bool live = false;
    bool ready = false;
    std::string detail;
};

[[nodiscard]] const char* lifecycle_state_to_string(ServiceLifecycleState state);
[[nodiscard]] ServiceHealthSnapshot stopped_health(std::string detail = "stopped");
[[nodiscard]] ServiceHealthSnapshot starting_health(std::string detail = "starting");
[[nodiscard]] ServiceHealthSnapshot ready_health(std::string detail = "ready");
[[nodiscard]] ServiceHealthSnapshot draining_health(std::string detail = "draining");
[[nodiscard]] ServiceHealthSnapshot failed_health(std::string detail);

} // namespace signalroute
