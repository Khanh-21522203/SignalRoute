#pragma once

#include "admin_endpoint_handler.h"

#include <string>
#include <utility>
#include <vector>

namespace signalroute {

struct AdminHttpRequest {
    std::string method = "GET";
    std::string path = "/health";
    std::string accept = "application/json";
};

struct AdminHttpRoutes {
    std::string health_path = "/health";
    std::string health_alias_path = "/healthz";
    std::string readiness_path = "/ready";
    std::string readiness_alias_path = "/readyz";
    std::string metrics_path = "/metrics";
};

struct AdminHttpResponse {
    int status_code = 200;
    std::string reason_phrase;
    std::string content_type;
    std::string body;
    std::vector<std::pair<std::string, std::string>> headers;

    [[nodiscard]] bool ok() const { return status_code >= 200 && status_code < 300; }
};

class AdminHttpHandler {
public:
    explicit AdminHttpHandler(AdminEndpointHandler& endpoint_handler, AdminHttpRoutes routes = {});

    [[nodiscard]] AdminHttpResponse handle(AdminHttpRequest request) const;
    [[nodiscard]] const AdminHttpRoutes& routes() const;

private:
    [[nodiscard]] AdminHttpResponse endpoint_to_http(AdminEndpointResponse response, bool include_body) const;
    [[nodiscard]] AdminHttpResponse not_found(bool include_body) const;
    [[nodiscard]] AdminHttpResponse method_not_allowed(bool include_body) const;

    AdminEndpointHandler& endpoint_handler_;
    AdminHttpRoutes routes_;
};

[[nodiscard]] std::string reason_phrase_for_status(int status_code);
[[nodiscard]] std::string serialize_http_response(const AdminHttpResponse& response);

} // namespace signalroute
