#include "common/metrics/metrics.h"
#include "runtime/metrics_exporter.h"
#include "runtime/runtime_application.h"

#include <arpa/inet.h>
#include <cassert>
#include <cstring>
#include <cerrno>
#include <iostream>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

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
    }

    close_fd(fd);
    return response;
}

std::string get(uint16_t port, const std::string& path) {
    return raw_http_request(
        port,
        "GET " + path + " HTTP/1.1\r\nHost: 127.0.0.1\r\nAccept: text/plain\r\n\r\n");
}

bool operation_not_permitted(const std::runtime_error& ex) {
    return std::string(ex.what()).find("Operation not permitted") != std::string::npos;
}

} // namespace

void test_exporter_serves_prometheus_text() {
    auto& metrics = signalroute::Metrics::instance();
    metrics.reset_for_test();
    metrics.inc_events_accepted(5);

    signalroute::MetricsExporter exporter;
    try {
        exporter.start({"127.0.0.1", 0, "/metrics", 4, 250, 4096});
    } catch (const std::runtime_error& ex) {
        if (operation_not_permitted(ex)) {
            assert(exporter.health_snapshot().state == signalroute::ServiceLifecycleState::Failed);
            return;
        }
        throw;
    }

    assert(exporter.is_running());
    assert(exporter.bound_port() > 0);
    assert(exporter.health_snapshot().ready);

    const auto response = get(exporter.bound_port(), "/metrics");
    assert(response.find("HTTP/1.1 200 OK\r\n") == 0);
    assert(response.find("Content-Type: text/plain; version=0.0.4") != std::string::npos);
    assert(response.find("# SignalRoute metrics fallback") != std::string::npos);
    assert(response.find("events_accepted_total 5") != std::string::npos);
    assert(exporter.accepted_connections() == 1);

    exporter.stop();
}

void test_exporter_rejects_unknown_path() {
    signalroute::MetricsExporter exporter;
    try {
        exporter.start({"127.0.0.1", 0, "/metrics", 4, 250, 4096});
    } catch (const std::runtime_error& ex) {
        if (operation_not_permitted(ex)) {
            assert(exporter.health_snapshot().state == signalroute::ServiceLifecycleState::Failed);
            return;
        }
        throw;
    }

    const auto response = get(exporter.bound_port(), "/health");
    assert(response.find("HTTP/1.1 404 Not Found\r\n") == 0);
    assert(response.find("metrics path not found") != std::string::npos);

    exporter.stop();
}

void test_exporter_stop_updates_lifecycle_health() {
    signalroute::MetricsExporter exporter;
    try {
        exporter.start({"127.0.0.1", 0, "/metrics", 4, 250, 4096});
    } catch (const std::runtime_error& ex) {
        if (operation_not_permitted(ex)) {
            assert(exporter.health_snapshot().state == signalroute::ServiceLifecycleState::Failed);
            return;
        }
        throw;
    }

    exporter.stop();
    assert(!exporter.is_running());
    assert(exporter.health_snapshot().state == signalroute::ServiceLifecycleState::Stopped);
}

void test_exporter_startup_failure_has_endpoint_diagnostics() {
    signalroute::MetricsExporter exporter;
    bool thrown = false;
    try {
        exporter.start({"not-an-ip", 0, "/metrics", 4, 250, 4096});
    } catch (const std::runtime_error& ex) {
        thrown = true;
        if (operation_not_permitted(ex)) {
            assert(exporter.health_snapshot().state == signalroute::ServiceLifecycleState::Failed);
            return;
        }
        const std::string message = ex.what();
        assert(message.find("metrics exporter listen_addr must be an IPv4 address") != std::string::npos);
        assert(message.find("listen_addr=not-an-ip") != std::string::npos);
        assert(message.find("path=/metrics") != std::string::npos);
    }

    assert(thrown);
    assert(exporter.health_snapshot().state == signalroute::ServiceLifecycleState::Failed);
}

void test_runtime_can_own_metrics_exporter_when_enabled() {
    auto config = config_for_role("query");
    config.observability.metrics_exporter_enabled = true;
    config.observability.metrics_addr = "127.0.0.1";
    config.observability.metrics_port = 0;
    config.observability.metrics_path = "/ops/metrics";

    signalroute::RuntimeApplication app;
    try {
        app.start(config);
    } catch (const std::runtime_error& ex) {
        if (operation_not_permitted(ex)) {
            assert(app.startup_failed());
            assert(!app.is_running());
            return;
        }
        throw;
    }

    assert(app.metrics_exporter_enabled());
    assert(app.metrics_exporter_running());
    assert(app.metrics_exporter_bound_port() > 0);
    assert(app.metrics_exporter_health_snapshot().ready);
    assert(app.admin().health().component_healthy("metrics_exporter"));

    signalroute::Metrics::instance().reset_for_test();
    signalroute::Metrics::instance().inc_ingest_queued(3);
    const auto response = get(app.metrics_exporter_bound_port(), "/ops/metrics");
    assert(response.find("HTTP/1.1 200 OK\r\n") == 0);
    assert(response.find("ingest_queued_total 3") != std::string::npos);

    app.stop();
    assert(!app.metrics_exporter_running());
}

int main() {
    std::cout << "test_metrics_exporter:\n";
    test_exporter_serves_prometheus_text();
    test_exporter_rejects_unknown_path();
    test_exporter_stop_updates_lifecycle_health();
    test_exporter_startup_failure_has_endpoint_diagnostics();
    test_runtime_can_own_metrics_exporter_when_enabled();
    std::cout << "All metrics exporter tests passed.\n";
    return 0;
}
