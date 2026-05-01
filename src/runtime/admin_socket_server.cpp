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
#include <unistd.h>

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

void send_all(int fd, const std::string& data) {
    std::size_t sent = 0;
    while (sent < data.size()) {
        const auto written = ::send(fd, data.data() + sent, data.size() - sent, 0);
        if (written <= 0) {
            return;
        }
        sent += static_cast<std::size_t>(written);
    }
}

} // namespace

AdminSocketServer::AdminSocketServer(AdminRequestLoop& loop) : loop_(loop) {}

AdminSocketServer::~AdminSocketServer() {
    stop();
}

void AdminSocketServer::start(AdminSocketEndpoint endpoint) {
    if (running_) {
        return;
    }
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
    addr.sin_port = htons(endpoint.port);
    if (::inet_pton(AF_INET, endpoint.listen_addr.c_str(), &addr.sin_addr) != 1) {
        lifecycle_state_.store(ServiceLifecycleState::Failed);
        close_fd(listen_fd_);
        throw std::runtime_error("admin socket listen_addr must be an IPv4 address");
    }

    if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        lifecycle_state_.store(ServiceLifecycleState::Failed);
        close_fd(listen_fd_);
        throw_socket_error("admin socket bind");
    }
    if (::listen(listen_fd_, endpoint.backlog) != 0) {
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
    std::string raw;
    raw.reserve(1024);
    char buffer[1024];
    while (raw.find("\r\n\r\n") == std::string::npos && raw.size() < 8192) {
        const auto read = ::recv(client_fd, buffer, sizeof(buffer), 0);
        if (read <= 0) {
            break;
        }
        raw.append(buffer, static_cast<std::size_t>(read));
    }
    const auto response = loop_.handle(parse_http_request(raw));
    send_all(client_fd, serialize_http_response(response));
}

} // namespace signalroute
