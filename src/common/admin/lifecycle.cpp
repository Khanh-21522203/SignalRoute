#include "lifecycle.h"

#include <utility>

namespace signalroute {

const char* lifecycle_state_to_string(ServiceLifecycleState state) {
    switch (state) {
        case ServiceLifecycleState::Stopped: return "stopped";
        case ServiceLifecycleState::Starting: return "starting";
        case ServiceLifecycleState::Ready: return "ready";
        case ServiceLifecycleState::Draining: return "draining";
        case ServiceLifecycleState::Failed: return "failed";
    }
    return "unknown";
}

ServiceHealthSnapshot stopped_health(std::string detail) {
    return ServiceHealthSnapshot{ServiceLifecycleState::Stopped, false, false, std::move(detail)};
}

ServiceHealthSnapshot starting_health(std::string detail) {
    return ServiceHealthSnapshot{ServiceLifecycleState::Starting, true, false, std::move(detail)};
}

ServiceHealthSnapshot ready_health(std::string detail) {
    return ServiceHealthSnapshot{ServiceLifecycleState::Ready, true, true, std::move(detail)};
}

ServiceHealthSnapshot draining_health(std::string detail) {
    return ServiceHealthSnapshot{ServiceLifecycleState::Draining, true, false, std::move(detail)};
}

ServiceHealthSnapshot failed_health(std::string detail) {
    return ServiceHealthSnapshot{ServiceLifecycleState::Failed, false, false, std::move(detail)};
}

} // namespace signalroute
