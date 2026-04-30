#pragma once

#include "common/admin/admin_http_handler.h"
#include "common/admin/lifecycle.h"

#include <atomic>
#include <cstddef>

namespace signalroute {

class RuntimeApplication;

class AdminRequestLoop {
public:
    explicit AdminRequestLoop(RuntimeApplication& runtime);
    ~AdminRequestLoop();

    AdminRequestLoop(const AdminRequestLoop&) = delete;
    AdminRequestLoop& operator=(const AdminRequestLoop&) = delete;

    void start();
    void stop();

    [[nodiscard]] bool is_running() const;
    [[nodiscard]] bool is_ready() const;
    [[nodiscard]] std::size_t handled_requests() const;
    [[nodiscard]] ServiceHealthSnapshot health_snapshot() const;
    [[nodiscard]] AdminHttpResponse handle(AdminHttpRequest request);

private:
    RuntimeApplication& runtime_;
    std::atomic<bool> running_{false};
    std::atomic<ServiceLifecycleState> lifecycle_state_{ServiceLifecycleState::Stopped};
    std::atomic<std::size_t> handled_requests_{0};
};

} // namespace signalroute
