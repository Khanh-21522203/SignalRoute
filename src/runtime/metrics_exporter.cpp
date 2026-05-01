#include "metrics_exporter.h"

#include "common/admin/admin_http_handler.h"
#include "common/metrics/metrics.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <utility>

namespace signalroute {
namespace {

void close_fd(int& fd) {
    if (fd >= 0) {
        ::close(fd);
        fd = -1;
    }
}

std::string endpoint_detail(const MetricsExporterEndpoint& endpoint) {
    return "listen_addr=" + endpoint.listen_addr +
           " port=" + std::to_string(endpoint.port) +
           " path=" + endpoint.path +
           " backlog=" + std::to_string(endpoint.backlog);
}

[[noreturn]] void throw_socket_error(const std::string& operation, const MetricsExporterEndpoint& endpoint) {
    throw std::runtime_error(operation + " failed for " + endpoint_detail(endpoint) + ": " + std::strerror(errno));
}

AdminHttpRequest parse_http_request(const std::string& raw) {
    AdminHttpRequest request;
    const auto line_end = raw.find("\r\n");
    const std::string request_line = raw.substr(0, line_end == std::string::npos ? raw.size() : line_end);

    const auto first_space = request_line.find(' ');
    const auto second_space = first_space == std::string::npos
        ? std::string::npos
        : request_line.find(' ', first_space + 1);
    if (first_space == std::string::npos || second_space == std::string::npos) {
        request.method = "GET";
        request.path = "/";
        return request;
    }

    request.method = request_line.substr(0, first_space);
    request.path = request_line.substr(first_space + 1, second_space - first_space - 1);
    if (request.path.empty()) {
        request.path = "/";
    }
    return request;
}

AdminHttpResponse text_response(const std::string& body, bool include_body) {
    AdminHttpResponse response;
    response.status_code = 200;
    response.reason_phrase = reason_phrase_for_status(response.status_code);
    response.content_type = "text/plain; version=0.0.4";
    response.body = include_body ? body : "";
    response.headers.emplace_back("Content-Type", response.content_type);
    response.headers.emplace_back("Content-Length", std::to_string(response.body.size()));
    response.headers.emplace_back("Cache-Control", "no-store");
    response.headers.emplace_back("Connection", "close");
    return response;
}

AdminHttpResponse error_response(int status_code, const std::string& body, bool include_body) {
    AdminHttpResponse response;
    response.status_code = status_code;
    response.reason_phrase = reason_phrase_for_status(status_code);
    response.content_type = "application/json";
    response.body = include_body ? body : "";
    response.headers.emplace_back("Content-Type", response.content_type);
    response.headers.emplace_back("Content-Length", std::to_string(response.body.size()));
    response.headers.emplace_back("Cache-Control", "no-store");
    response.headers.emplace_back("Connection", "close");
    return response;
}

void configure_read_timeout(int fd, int timeout_ms) {
    timeval timeout{};
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    if (::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0) {
        throw std::runtime_error(std::string("metrics exporter client read timeout failed: ") + std::strerror(errno));
    }
}

std::size_t send_all(int fd, const std::string& data) {
    std::size_t sent = 0;
    while (sent < data.size()) {
        const auto written = ::send(fd, data.data() + sent, data.size() - sent, 0);
        if (written <= 0) {
            return sent;
        }
        sent += static_cast<std::size_t>(written);
    }
    return sent;
}

} // namespace

MetricsExporter::~MetricsExporter() {
    stop();
}

void MetricsExporter::start(MetricsExporterEndpoint endpoint) {
    if (running_) {
        return;
    }
    if (endpoint.path.empty() || endpoint.path.front() != '/') {
        throw std::runtime_error("metrics exporter path must start with '/': " + endpoint.path);
    }
    if (endpoint.read_timeout_ms <= 0) {
        throw std::runtime_error("metrics exporter read_timeout_ms must be positive");
    }
    if (endpoint.max_request_bytes == 0) {
        throw std::runtime_error("metrics exporter max_request_bytes must be positive");
    }
    endpoint_ = std::move(endpoint);
    lifecycle_state_.store(ServiceLifecycleState::Starting);

    listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        lifecycle_state_.store(ServiceLifecycleState::Failed);
        throw_socket_error("metrics exporter create", endpoint_);
    }

    int reuse = 1;
    if (::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) != 0) {
        lifecycle_state_.store(ServiceLifecycleState::Failed);
        close_fd(listen_fd_);
        throw_socket_error("metrics exporter setsockopt", endpoint_);
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(endpoint_.port);
    if (::inet_pton(AF_INET, endpoint_.listen_addr.c_str(), &addr.sin_addr) != 1) {
        lifecycle_state_.store(ServiceLifecycleState::Failed);
        close_fd(listen_fd_);
        throw std::runtime_error("metrics exporter listen_addr must be an IPv4 address: " + endpoint_detail(endpoint_));
    }

    if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        lifecycle_state_.store(ServiceLifecycleState::Failed);
        close_fd(listen_fd_);
        throw_socket_error("metrics exporter bind", endpoint_);
    }
    if (::listen(listen_fd_, endpoint_.backlog) != 0) {
        lifecycle_state_.store(ServiceLifecycleState::Failed);
        close_fd(listen_fd_);
        throw_socket_error("metrics exporter listen", endpoint_);
    }

    sockaddr_in actual{};
    socklen_t actual_len = sizeof(actual);
    if (::getsockname(listen_fd_, reinterpret_cast<sockaddr*>(&actual), &actual_len) != 0) {
        lifecycle_state_.store(ServiceLifecycleState::Failed);
        close_fd(listen_fd_);
        throw_socket_error("metrics exporter getsockname", endpoint_);
    }
    bound_port_ = ntohs(actual.sin_port);

    running_.store(true);
    lifecycle_state_.store(ServiceLifecycleState::Ready);
    accept_thread_ = std::thread([this] { accept_loop(); });
}

void MetricsExporter::stop() {
    if (!running_ && listen_fd_ < 0) {
        return;
    }
    lifecycle_state_.store(ServiceLifecycleState::Draining);
    running_.store(false);
    if (listen_fd_ >= 0) {
        ::shutdown(listen_fd_, SHUT_RDWR);
    }
    close_fd(listen_fd_);
    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }
    lifecycle_state_.store(ServiceLifecycleState::Stopped);
}

bool MetricsExporter::is_running() const {
    return running_;
}

uint16_t MetricsExporter::bound_port() const {
    return bound_port_;
}

std::size_t MetricsExporter::accepted_connections() const {
    return accepted_connections_.load();
}

ServiceHealthSnapshot MetricsExporter::health_snapshot() const {
    switch (lifecycle_state_.load()) {
        case ServiceLifecycleState::Ready: return ready_health("metrics exporter accepting scrapes");
        case ServiceLifecycleState::Starting: return starting_health("metrics exporter starting");
        case ServiceLifecycleState::Draining: return draining_health("metrics exporter draining");
        case ServiceLifecycleState::Failed: return failed_health("metrics exporter failed");
        case ServiceLifecycleState::Stopped: return stopped_health("metrics exporter stopped");
    }
    return failed_health("metrics exporter unknown lifecycle state");
}

void MetricsExporter::accept_loop() {
    while (running_) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        const int client_fd = ::accept(listen_fd_, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
        if (client_fd < 0) {
            if (!running_ || errno == EBADF || errno == EINVAL) {
                break;
            }
            continue;
        }
        accepted_connections_.fetch_add(1);
        handle_client(client_fd);
        int fd = client_fd;
        close_fd(fd);
    }
}

void MetricsExporter::handle_client(int client_fd) {
    configure_read_timeout(client_fd, endpoint_.read_timeout_ms);

    std::string raw;
    raw.reserve(endpoint_.max_request_bytes < 1024 ? endpoint_.max_request_bytes : 1024);
    char buffer[1024];
    bool timed_out = false;
    while (raw.find("\r\n\r\n") == std::string::npos && raw.size() < endpoint_.max_request_bytes) {
        const auto remaining = endpoint_.max_request_bytes - raw.size();
        const auto read_size = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
        const auto read = ::recv(client_fd, buffer, read_size, 0);
        if (read <= 0) {
            if (read < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                timed_out = true;
            }
            break;
        }
        raw.append(buffer, static_cast<std::size_t>(read));
    }

    const bool complete = raw.find("\r\n\r\n") != std::string::npos;
    const bool payload_too_large = !complete && raw.size() >= endpoint_.max_request_bytes;
    const auto request = parse_http_request(raw);
    const bool include_body = request.method != "HEAD";

    AdminHttpResponse response;
    if (payload_too_large) {
        response = error_response(413, R"({"error":"metrics request too large"})", include_body);
    } else if (!complete && timed_out) {
        response = error_response(408, R"({"error":"metrics request timeout"})", include_body);
    } else if (request.method != "GET" && request.method != "HEAD") {
        response = error_response(405, R"({"error":"method not allowed"})", include_body);
    } else if (request.path != endpoint_.path) {
        response = error_response(404, R"({"error":"metrics path not found"})", include_body);
    } else {
        response = text_response(Metrics::instance().export_text(), include_body);
    }

    (void)send_all(client_fd, serialize_http_response(response));
}

} // namespace signalroute
