#include "runtime/admin_request_loop.h"
#include "runtime/admin_socket_server.h"
#include "runtime/runtime_application.h"

#include <arpa/inet.h>
#include <cassert>
#include <cstring>
#include <cerrno>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

namespace {

signalroute::Config config_for_role(const std::string& role) {
    signalroute::Config config;
    config.server.role = role;
    config.postgis.dsn = "host=localhost dbname=signalroute";
    config.geofence.eval_enabled = false;
    config.gateway.rate_limit_rps_per_device = 1000;
    config.gateway.timestamp_skew_tolerance_s = 60;
    return config;
}

void close_fd(int& fd) {
    if (fd >= 0) {
        ::close(fd);
        fd = -1;
    }
}

std::string raw_http_request(uint16_t port, const std::string& request);

std::string http_request(uint16_t port, const std::string& path) {
    const std::string request = "GET " + path + " HTTP/1.1\r\nHost: 127.0.0.1\r\nAccept: application/json\r\n\r\n";
    return raw_http_request(port, request);
}

std::string raw_http_request(uint16_t port, const std::string& request) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        throw std::runtime_error("socket failed");
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    assert(::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) == 1);
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        close_fd(fd);
        throw std::runtime_error(std::string("connect failed: ") + std::strerror(errno));
    }

    assert(::send(fd, request.data(), request.size(), 0) == static_cast<ssize_t>(request.size()));

    std::string response;
    char buffer[1024];
    while (true) {
        const auto read = ::recv(fd, buffer, sizeof(buffer), 0);
        if (read <= 0) {
            break;
        }
        response.append(buffer, static_cast<std::size_t>(read));
        if (response.find("\r\n\r\n") != std::string::npos && response.find("}") != std::string::npos) {
            break;
        }
    }

    close_fd(fd);
    return response;
}

} // namespace

void test_socket_serves_health_through_request_loop() {
    signalroute::RuntimeApplication runtime;
    runtime.start(config_for_role("query"));
    signalroute::AdminRequestLoop loop(runtime);
    signalroute::AdminSocketServer server(loop);

    try {
        server.start({"127.0.0.1", 0, 4});
    } catch (const std::runtime_error& ex) {
        const std::string message = ex.what();
        if (message.find("Operation not permitted") != std::string::npos) {
            assert(server.health_snapshot().state == signalroute::ServiceLifecycleState::Failed);
            return;
        }
        throw;
    }

    assert(server.is_running());
    assert(server.bound_port() > 0);
    assert(server.health_snapshot().ready);
    const auto response = http_request(server.bound_port(), "/health");

    assert(response.find("HTTP/1.1 200 OK\r\n") == 0);
    assert(response.find("\"role\":\"query\"") != std::string::npos);
    assert(server.accepted_connections() == 1);

    server.stop();
    assert(!server.is_running());
    assert(!server.health_snapshot().live);
}

void test_socket_serves_readiness_failure() {
    auto config = config_for_role("query");
    config.observability.require_kafka_readiness = true;
    signalroute::RuntimeApplication runtime;
    runtime.start(config);
    signalroute::AdminRequestLoop loop(runtime);
    signalroute::AdminSocketServer server(loop);

    try {
        server.start({"127.0.0.1", 0, 4});
    } catch (const std::runtime_error& ex) {
        const std::string message = ex.what();
        if (message.find("Operation not permitted") != std::string::npos) {
            assert(server.health_snapshot().state == signalroute::ServiceLifecycleState::Failed);
            return;
        }
        throw;
    }
    const auto response = http_request(server.bound_port(), "/ready");

    assert(response.find("HTTP/1.1 503 Service Unavailable\r\n") == 0);
    assert(response.find("\"name\":\"kafka\"") != std::string::npos);

    server.stop();
}

void test_socket_rejects_oversized_incomplete_request_and_logs() {
    signalroute::RuntimeApplication runtime;
    runtime.start(config_for_role("query"));
    signalroute::AdminRequestLoop loop(runtime);
    std::mutex logs_mu;
    std::vector<signalroute::AdminSocketAccessLogEntry> logs;
    signalroute::AdminSocketServer server(loop, [&](const signalroute::AdminSocketAccessLogEntry& entry) {
        std::lock_guard<std::mutex> lock(logs_mu);
        logs.push_back(entry);
    });

    try {
        server.start({"127.0.0.1", 0, 4, 250, 24});
    } catch (const std::runtime_error& ex) {
        const std::string message = ex.what();
        if (message.find("Operation not permitted") != std::string::npos) {
            assert(server.health_snapshot().state == signalroute::ServiceLifecycleState::Failed);
            return;
        }
        throw;
    }

    const auto response = raw_http_request(
        server.bound_port(),
        "GET /health HTTP/1.1\r\nHost: 127.0.0.1\r\nAccept: application/json\r\n\r\n");

    server.stop();
    assert(response.find("HTTP/1.1 413 Payload Too Large\r\n") == 0);
    assert(response.find("admin request too large") != std::string::npos);
    assert(logs.size() == 1);
    assert(logs.front().status_code == 413);
    assert(logs.front().payload_too_large);
    assert(!logs.front().timed_out);
}

void test_socket_times_out_incomplete_request_and_logs() {
    signalroute::RuntimeApplication runtime;
    runtime.start(config_for_role("query"));
    signalroute::AdminRequestLoop loop(runtime);
    std::mutex logs_mu;
    std::vector<signalroute::AdminSocketAccessLogEntry> logs;
    signalroute::AdminSocketServer server(loop, [&](const signalroute::AdminSocketAccessLogEntry& entry) {
        std::lock_guard<std::mutex> lock(logs_mu);
        logs.push_back(entry);
    });

    try {
        server.start({"127.0.0.1", 0, 4, 50, 256});
    } catch (const std::runtime_error& ex) {
        const std::string message = ex.what();
        if (message.find("Operation not permitted") != std::string::npos) {
            assert(server.health_snapshot().state == signalroute::ServiceLifecycleState::Failed);
            return;
        }
        throw;
    }

    const auto response = raw_http_request(
        server.bound_port(),
        "GET /health HTTP/1.1\r\nHost: 127.0.0.1\r\n");

    server.stop();
    assert(response.find("HTTP/1.1 408 Request Timeout\r\n") == 0);
    assert(response.find("admin request timeout") != std::string::npos);
    assert(logs.size() == 1);
    assert(logs.front().status_code == 408);
    assert(logs.front().timed_out);
    assert(!logs.front().payload_too_large);
}

void test_socket_access_log_event_is_structured_logfmt_ready() {
    const auto event = signalroute::make_admin_socket_access_log_event({
        "GET",
        "/health",
        200,
        64,
        128,
        false,
        false,
    });
    const auto line = signalroute::format_logfmt(event);

    assert(line.find("component=admin") != std::string::npos);
    assert(line.find("event=admin.socket_request") != std::string::npos);
    assert(line.find("method=GET") != std::string::npos);
    assert(line.find("path=/health") != std::string::npos);
    assert(line.find("status=200") != std::string::npos);
}

int main() {
    std::cout << "test_admin_socket_server:\n";
    test_socket_serves_health_through_request_loop();
    test_socket_serves_readiness_failure();
    test_socket_rejects_oversized_incomplete_request_and_logs();
    test_socket_times_out_incomplete_request_and_logs();
    test_socket_access_log_event_is_structured_logfmt_ready();
    std::cout << "All admin socket server tests passed.\n";
    return 0;
}
