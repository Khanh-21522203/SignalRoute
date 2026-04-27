#include "admin_endpoint_handler.h"

#include <cstddef>
#include <sstream>

namespace signalroute {
namespace {

std::string json_escape(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 2);
    for (const char ch : value) {
        switch (ch) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(ch); break;
        }
    }
    return out;
}

void append_json_string(std::ostringstream& out, const std::string& value) {
    out << '"' << json_escape(value) << '"';
}

} // namespace

AdminEndpointHandler::AdminEndpointHandler(AdminService& admin) : admin_(admin) {}

AdminEndpointResponse AdminEndpointHandler::handle_health(AdminEndpointRequest /*request*/) const {
    const auto health = admin_.health();
    return AdminEndpointResponse{
        health.healthy ? 200 : 503,
        "application/json",
        serialize_health_json(health),
    };
}

AdminEndpointResponse AdminEndpointHandler::handle_metrics(AdminEndpointRequest /*request*/) const {
    const auto metrics = admin_.metrics();
    return AdminEndpointResponse{
        200,
        "text/plain; version=0.0.4",
        metrics.metrics_text,
    };
}

std::string serialize_health_json(const HealthResponse& health) {
    std::ostringstream out;
    out << '{';
    out << "\"healthy\":" << (health.healthy ? "true" : "false");
    out << ",\"role\":";
    append_json_string(out, health.role);
    out << ",\"version\":";
    append_json_string(out, health.version);
    out << ",\"uptime_s\":" << health.uptime_s;
    out << ",\"components\":[";
    for (std::size_t i = 0; i < health.components.size(); ++i) {
        const auto& component = health.components[i];
        if (i > 0) {
            out << ',';
        }
        out << '{';
        out << "\"name\":";
        append_json_string(out, component.name);
        out << ",\"healthy\":" << (component.healthy ? "true" : "false");
        out << ",\"required\":" << (component.required ? "true" : "false");
        out << ",\"detail\":";
        append_json_string(out, component.detail);
        out << '}';
    }
    out << "]}";
    return out.str();
}

} // namespace signalroute
