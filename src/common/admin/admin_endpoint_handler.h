#pragma once

#include "admin_service.h"

#include <string>

namespace signalroute {

struct AdminEndpointRequest {
    std::string accept = "application/json";
};

struct AdminEndpointResponse {
    int status_code = 200;
    std::string content_type;
    std::string body;

    [[nodiscard]] bool ok() const { return status_code >= 200 && status_code < 300; }
};

class AdminEndpointHandler {
public:
    explicit AdminEndpointHandler(AdminService& admin);

    [[nodiscard]] AdminEndpointResponse handle_health(AdminEndpointRequest request = {}) const;
    [[nodiscard]] AdminEndpointResponse handle_metrics(AdminEndpointRequest request = {}) const;

private:
    AdminService& admin_;
};

[[nodiscard]] std::string serialize_health_json(const HealthResponse& health);

} // namespace signalroute
