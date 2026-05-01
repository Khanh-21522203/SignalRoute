#pragma once

#include "common/admin/lifecycle.h"
#include "common/logging/structured_log.h"

#include <atomic>
#include <cstdint>
#include <cstddef>
#include <functional>
#include <string>
#include <thread>

namespace signalroute {

class AdminRequestLoop;

struct AdminSocketEndpoint {
    std::string listen_addr = "127.0.0.1";
    uint16_t port = 0;
    int backlog = 16;
    int read_timeout_ms = 1000;
    std::size_t max_request_bytes = 8192;
};

struct AdminSocketAccessLogEntry {
    std::string method;
    std::string path;
    int status_code = 0;
    std::size_t request_bytes = 0;
    std::size_t response_bytes = 0;
    bool timed_out = false;
    bool payload_too_large = false;
};

using AdminSocketAccessLogSink = std::function<void(const AdminSocketAccessLogEntry&)>;

class AdminSocketServer {
public:
    explicit AdminSocketServer(AdminRequestLoop& loop, AdminSocketAccessLogSink access_log_sink = {});
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
    AdminSocketEndpoint endpoint_;
    AdminSocketAccessLogSink access_log_sink_;
    std::thread accept_thread_;
};

[[nodiscard]] StructuredLogEvent make_admin_socket_access_log_event(const AdminSocketAccessLogEntry& entry);

} // namespace signalroute
