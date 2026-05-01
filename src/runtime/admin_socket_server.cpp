#include "admin_socket_server.h"

#include "admin_request_loop.h"
#include "common/admin/admin_http_handler.h"

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

[[noreturn]] void throw_socket_error(const std::string& operation) {
    throw std::runtime_error(operation + " failed: " + std::strerror(errno));
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
        request.path = "/health";
        return request;
    }

    request.method = request_line.substr(0, first_space);
    request.path = request_line.substr(first_space + 1, second_space - first_space - 1);
    if (request.path.empty()) {
        request.path = "/health";
    }

    const std::string accept_prefix = "Accept:";
    std::size_t pos = raw.find(accept_prefix);
    if (pos != std::string::npos) {
        pos += accept_prefix.size();
        while (pos < raw.size() && raw[pos] == ' ') {
            ++pos;
        }
        const auto accept_end = raw.find("\r\n", pos);
        request.accept = raw.substr(pos, accept_end == std::string::npos ? std::string::npos : accept_end - pos);
    }
    return request;
}

AdminHttpResponse admin_error_response(int status_code, const std::string& body) {
    AdminHttpResponse response;
    response.status_code = status_code;
    response.reason_phrase = reason_phrase_for_status(status_code);
    response.content_type = "application/json";
    response.body = body;
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
        throw_socket_error("admin socket client read timeout");
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

AdminSocketServer::AdminSocketServer(AdminRequestLoop& loop, AdminSocketAccessLogSink access_log_sink)
    : loop_(loop), access_log_sink_(std::move(access_log_sink)) {}

AdminSocketServer::~AdminSocketServer() {
    stop();
}

void AdminSocketServer::start(AdminSocketEndpoint endpoint) {
    if (running_) {
        return;
    }
    if (endpoint.read_timeout_ms <= 0) {
        throw std::runtime_error("admin socket read_timeout_ms must be positive");
    }
    if (endpoint.max_request_bytes == 0) {
        throw std::runtime_error("admin socket max_request_bytes must be positive");
    }
    endpoint_ = std::move(endpoint);
    lifecycle_state_.store(ServiceLifecycleState::Starting);

    listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        lifecycle_state_.store(ServiceLifecycleState::Failed);
        throw_socket_error("admin socket create");
    }

    int reuse = 1;
    if (::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) != 0) {
        lifecycle_state_.store(ServiceLifecycleState::Failed);
        close_fd(listen_fd_);
        throw_socket_error("admin socket setsockopt");
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(endpoint_.port);
    if (::inet_pton(AF_INET, endpoint_.listen_addr.c_str(), &addr.sin_addr) != 1) {
        lifecycle_state_.store(ServiceLifecycleState::Failed);
        close_fd(listen_fd_);
        throw std::runtime_error("admin socket listen_addr must be an IPv4 address");
    }

    if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        lifecycle_state_.store(ServiceLifecycleState::Failed);
        close_fd(listen_fd_);
        throw_socket_error("admin socket bind");
    }
    if (::listen(listen_fd_, endpoint_.backlog) != 0) {
        lifecycle_state_.store(ServiceLifecycleState::Failed);
        close_fd(listen_fd_);
        throw_socket_error("admin socket listen");
    }

    sockaddr_in actual{};
    socklen_t actual_len = sizeof(actual);
    if (::getsockname(listen_fd_, reinterpret_cast<sockaddr*>(&actual), &actual_len) != 0) {
        lifecycle_state_.store(ServiceLifecycleState::Failed);
        close_fd(listen_fd_);
        throw_socket_error("admin socket getsockname");
    }
    bound_port_ = ntohs(actual.sin_port);

    loop_.start();
    running_.store(true);
    lifecycle_state_.store(ServiceLifecycleState::Ready);
    accept_thread_ = std::thread([this] { accept_loop(); });
}

void AdminSocketServer::stop() {
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
    loop_.stop();
    lifecycle_state_.store(ServiceLifecycleState::Stopped);
}

bool AdminSocketServer::is_running() const {
    return running_;
}

uint16_t AdminSocketServer::bound_port() const {
    return bound_port_;
}

std::size_t AdminSocketServer::accepted_connections() const {
    return accepted_connections_.load();
}

ServiceHealthSnapshot AdminSocketServer::health_snapshot() const {
    switch (lifecycle_state_.load()) {
        case ServiceLifecycleState::Ready: return ready_health("admin socket accepting connections");
        case ServiceLifecycleState::Starting: return starting_health("admin socket starting");
        case ServiceLifecycleState::Draining: return draining_health("admin socket draining");
        case ServiceLifecycleState::Failed: return failed_health("admin socket failed");
        case ServiceLifecycleState::Stopped: return stopped_health("admin socket stopped");
    }
    return failed_health("admin socket unknown lifecycle state");
}

void AdminSocketServer::accept_loop() {
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

void AdminSocketServer::handle_client(int client_fd) {
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
    AdminHttpRequest request = parse_http_request(raw);
    AdminHttpResponse response;
    if (payload_too_large) {
        response = admin_error_response(413, R"({"error":"admin request too large"})");
    } else if (!complete && timed_out) {
        response = admin_error_response(408, R"({"error":"admin request timeout"})");
    } else {
        response = loop_.handle(request);
    }
    const auto serialized = serialize_http_response(response);
    const auto sent = send_all(client_fd, serialized);

    if (access_log_sink_) {
        access_log_sink_(AdminSocketAccessLogEntry{
            request.method,
            request.path,
            response.status_code,
            raw.size(),
            sent,
            timed_out && !complete,
            payload_too_large,
        });
    }
}

StructuredLogEvent make_admin_socket_access_log_event(const AdminSocketAccessLogEntry& entry) {
    return make_log_event(
        "info",
        "admin",
        "admin.socket_request",
        "admin socket request handled",
        {
            {"method", entry.method.empty() ? "unknown" : entry.method},
            {"path", entry.path.empty() ? "unknown" : entry.path},
            {"status", std::to_string(entry.status_code)},
            {"request_bytes", std::to_string(entry.request_bytes)},
            {"response_bytes", std::to_string(entry.response_bytes)},
            {"timed_out", entry.timed_out ? "true" : "false"},
            {"payload_too_large", entry.payload_too_large ? "true" : "false"},
        });
}

} // namespace signalroute
