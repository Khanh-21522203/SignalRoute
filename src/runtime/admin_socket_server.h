#pragma once

#include "common/admin/lifecycle.h"

#include <atomic>
#include <cstdint>
#include <cstddef>
#include <string>
#include <thread>

namespace signalroute {

class AdminRequestLoop;

struct AdminSocketEndpoint {
    std::string listen_addr = "127.0.0.1";
    uint16_t port = 0;
    int backlog = 16;
};

class AdminSocketServer {
public:
    explicit AdminSocketServer(AdminRequestLoop& loop);
    ~AdminSocketServer();

    AdminSocketServer(const AdminSocketServer&) = delete;
    AdminSocketServer& operator=(const AdminSocketServer&) = delete;

    void start(AdminSocketEndpoint endpoint);
    void stop();

    [[nodiscard]] bool is_running() const;
    [[nodiscard]] uint16_t bound_port() const;
    [[nodiscard]] std::size_t accepted_connections() const;
    [[nodiscard]] ServiceHealthSnapshot health_snapshot() const;

private:
    void accept_loop();
    void handle_client(int client_fd);

    AdminRequestLoop& loop_;
    std::atomic<bool> running_{false};
    std::atomic<ServiceLifecycleState> lifecycle_state_{ServiceLifecycleState::Stopped};
    std::atomic<std::size_t> accepted_connections_{0};
    int listen_fd_ = -1;
    uint16_t bound_port_ = 0;
    std::thread accept_thread_;
};

} // namespace signalroute
