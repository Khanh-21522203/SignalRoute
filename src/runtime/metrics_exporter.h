#pragma once

#include "common/admin/lifecycle.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <thread>

namespace signalroute {

struct MetricsExporterEndpoint {
    std::string listen_addr = "127.0.0.1";
    uint16_t port = 0;
    std::string path = "/metrics";
    int backlog = 16;
    int read_timeout_ms = 1000;
    std::size_t max_request_bytes = 8192;
};

class MetricsExporter {
public:
    MetricsExporter() = default;
    ~MetricsExporter();

    MetricsExporter(const MetricsExporter&) = delete;
    MetricsExporter& operator=(const MetricsExporter&) = delete;

    void start(MetricsExporterEndpoint endpoint);
    void stop();

    [[nodiscard]] bool is_running() const;
    [[nodiscard]] uint16_t bound_port() const;
    [[nodiscard]] std::size_t accepted_connections() const;
    [[nodiscard]] ServiceHealthSnapshot health_snapshot() const;

private:
    void accept_loop();
    void handle_client(int client_fd);

    std::atomic<bool> running_{false};
    std::atomic<ServiceLifecycleState> lifecycle_state_{ServiceLifecycleState::Stopped};
    std::atomic<std::size_t> accepted_connections_{0};
    int listen_fd_ = -1;
    uint16_t bound_port_ = 0;
    MetricsExporterEndpoint endpoint_;
    std::thread accept_thread_;
};

} // namespace signalroute
