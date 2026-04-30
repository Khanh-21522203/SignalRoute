#include "admin_request_loop.h"

#include "runtime_application.h"

#include <string>
#include <utility>

namespace signalroute {
namespace {

AdminHttpResponse loop_stopped_response() {
    AdminHttpResponse response;
    response.status_code = 503;
    response.reason_phrase = reason_phrase_for_status(response.status_code);
    response.content_type = "application/json";
    response.body = R"({"error":"admin request loop stopped"})";
    response.headers.emplace_back("Content-Type", response.content_type);
    response.headers.emplace_back("Content-Length", std::to_string(response.body.size()));
    response.headers.emplace_back("Cache-Control", "no-store");
    return response;
}

} // namespace

AdminRequestLoop::AdminRequestLoop(RuntimeApplication& runtime) : runtime_(runtime) {}

AdminRequestLoop::~AdminRequestLoop() {
    if (running_) {
        stop();
    }
}

void AdminRequestLoop::start() {
    if (running_) {
        return;
    }
    lifecycle_state_.store(ServiceLifecycleState::Starting);
    running_.store(true);
    lifecycle_state_.store(ServiceLifecycleState::Ready);
}

void AdminRequestLoop::stop() {
    if (!running_) {
        return;
    }
    lifecycle_state_.store(ServiceLifecycleState::Draining);
    running_.store(false);
    lifecycle_state_.store(ServiceLifecycleState::Stopped);
}

bool AdminRequestLoop::is_running() const {
    return running_;
}

bool AdminRequestLoop::is_ready() const {
    return lifecycle_state_.load() == ServiceLifecycleState::Ready;
}

std::size_t AdminRequestLoop::handled_requests() const {
    return handled_requests_.load();
}

ServiceHealthSnapshot AdminRequestLoop::health_snapshot() const {
    switch (lifecycle_state_.load()) {
        case ServiceLifecycleState::Ready: return ready_health("admin request loop accepting requests");
        case ServiceLifecycleState::Starting: return starting_health("admin request loop starting");
        case ServiceLifecycleState::Draining: return draining_health("admin request loop draining");
        case ServiceLifecycleState::Failed: return failed_health("admin request loop failed");
        case ServiceLifecycleState::Stopped: return stopped_health("admin request loop stopped");
    }
    return failed_health("admin request loop unknown lifecycle state");
}

AdminHttpResponse AdminRequestLoop::handle(AdminHttpRequest request) {
    if (!running_) {
        return loop_stopped_response();
    }
    handled_requests_.fetch_add(1);
    return runtime_.handle_admin_http(std::move(request));
}

} // namespace signalroute
