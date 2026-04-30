#include "admin_http_handler.h"

#include <sstream>
#include <utility>

namespace signalroute {
namespace {

bool is_health_path(const std::string& path) {
    return path == "/health" || path == "/healthz" || path == "/ready" || path == "/readyz";
}

bool is_metrics_path(const std::string& path) {
    return path == "/metrics";
}

bool method_can_route(const std::string& method) {
    return method == "GET" || method == "HEAD";
}

void add_standard_headers(AdminHttpResponse& response) {
    response.headers.emplace_back("Content-Type", response.content_type);
    response.headers.emplace_back("Content-Length", std::to_string(response.body.size()));
    response.headers.emplace_back("Cache-Control", "no-store");
}

} // namespace

AdminHttpHandler::AdminHttpHandler(AdminEndpointHandler& endpoint_handler)
    : endpoint_handler_(endpoint_handler) {}

AdminHttpResponse AdminHttpHandler::handle(AdminHttpRequest request) const {
    const bool include_body = request.method != "HEAD";
    if (!method_can_route(request.method)) {
        return method_not_allowed(include_body);
    }
    if (is_health_path(request.path)) {
        return endpoint_to_http(endpoint_handler_.handle_health({request.accept}), include_body);
    }
    if (is_metrics_path(request.path)) {
        return endpoint_to_http(endpoint_handler_.handle_metrics({request.accept}), include_body);
    }
    return not_found(include_body);
}

AdminHttpResponse AdminHttpHandler::endpoint_to_http(AdminEndpointResponse response, bool include_body) const {
    AdminHttpResponse out;
    out.status_code = response.status_code;
    out.reason_phrase = reason_phrase_for_status(response.status_code);
    out.content_type = response.content_type;
    out.body = include_body ? std::move(response.body) : std::string{};
    add_standard_headers(out);
    return out;
}

AdminHttpResponse AdminHttpHandler::not_found(bool include_body) const {
    AdminHttpResponse out;
    out.status_code = 404;
    out.reason_phrase = reason_phrase_for_status(out.status_code);
    out.content_type = "application/json";
    out.body = include_body ? R"({"error":"not found"})" : std::string{};
    add_standard_headers(out);
    return out;
}

AdminHttpResponse AdminHttpHandler::method_not_allowed(bool include_body) const {
    AdminHttpResponse out;
    out.status_code = 405;
    out.reason_phrase = reason_phrase_for_status(out.status_code);
    out.content_type = "application/json";
    out.body = include_body ? R"({"error":"method not allowed"})" : std::string{};
    add_standard_headers(out);
    out.headers.emplace_back("Allow", "GET, HEAD");
    return out;
}

std::string reason_phrase_for_status(int status_code) {
    switch (status_code) {
        case 200: return "OK";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 503: return "Service Unavailable";
        default: return "Unknown";
    }
}

std::string serialize_http_response(const AdminHttpResponse& response) {
    std::ostringstream out;
    out << "HTTP/1.1 " << response.status_code << ' ' << response.reason_phrase << "\r\n";
    for (const auto& [name, value] : response.headers) {
        out << name << ": " << value << "\r\n";
    }
    out << "\r\n" << response.body;
    return out.str();
}

} // namespace signalroute
