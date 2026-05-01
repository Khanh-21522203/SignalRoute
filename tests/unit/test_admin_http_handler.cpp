#include "common/admin/admin_http_handler.h"
#include "common/metrics/metrics.h"

#include <cassert>
#include <iostream>
#include <string>

namespace {

signalroute::AdminHttpHandler make_handler(signalroute::AdminEndpointHandler& endpoint) {
    return signalroute::AdminHttpHandler(endpoint);
}

bool has_header(const signalroute::AdminHttpResponse& response,
                const std::string& name,
                const std::string& value) {
    for (const auto& [header_name, header_value] : response.headers) {
        if (header_name == name && header_value == value) {
            return true;
        }
    }
    return false;
}

} // namespace

void test_health_route_returns_json_200() {
    signalroute::AdminService admin("standalone", "test-version");
    admin.register_component("runtime", [] {
        return signalroute::ComponentHealth{"runtime", true, true, "ready"};
    });
    signalroute::AdminEndpointHandler endpoint(admin);
    auto handler = make_handler(endpoint);

    const auto response = handler.handle({"GET", "/health", "application/json"});

    assert(response.ok());
    assert(response.status_code == 200);
    assert(response.reason_phrase == "OK");
    assert(response.content_type == "application/json");
    assert(response.body.find("\"healthy\":true") != std::string::npos);
    assert(has_header(response, "Cache-Control", "no-store"));
}

void test_ready_alias_maps_to_readiness_and_unhealthy_returns_503() {
    signalroute::AdminService admin("query");
    admin.register_readiness_component("redis", [] {
        return signalroute::ComponentHealth{"redis", false, true, "down"};
    });
    signalroute::AdminEndpointHandler endpoint(admin);
    signalroute::AdminHttpHandler handler(endpoint);

    const auto health = handler.handle({"GET", "/health", "application/json"});
    const auto response = handler.handle({"GET", "/readyz", "application/json"});

    assert(health.ok());
    assert(health.status_code == 200);
    assert(!response.ok());
    assert(response.status_code == 503);
    assert(response.reason_phrase == "Service Unavailable");
    assert(response.body.find("\"healthy\":false") != std::string::npos);
}

void test_metrics_route_returns_prometheus_text() {
    auto& metrics = signalroute::Metrics::instance();
    metrics.reset_for_test();
    metrics.inc_events_accepted(4);
    signalroute::AdminService admin("standalone");
    signalroute::AdminEndpointHandler endpoint(admin);
    signalroute::AdminHttpHandler handler(endpoint);

    const auto response = handler.handle({"GET", "/metrics", "text/plain"});

    assert(response.ok());
    assert(response.status_code == 200);
    assert(response.content_type == "text/plain; version=0.0.4");
    assert(response.body.find("events_accepted_total 4") != std::string::npos);
}

void test_head_returns_headers_without_body() {
    signalroute::AdminService admin("standalone");
    signalroute::AdminEndpointHandler endpoint(admin);
    signalroute::AdminHttpHandler handler(endpoint);

    const auto response = handler.handle({"HEAD", "/health", "application/json"});

    assert(response.status_code == 200);
    assert(response.body.empty());
    assert(has_header(response, "Content-Length", "0"));
}

void test_unknown_path_returns_404() {
    signalroute::AdminService admin("standalone");
    signalroute::AdminEndpointHandler endpoint(admin);
    signalroute::AdminHttpHandler handler(endpoint);

    const auto response = handler.handle({"GET", "/missing", "application/json"});

    assert(response.status_code == 404);
    assert(response.reason_phrase == "Not Found");
    assert(response.body == R"({"error":"not found"})");
}

void test_unsupported_method_returns_405_with_allow_header() {
    signalroute::AdminService admin("standalone");
    signalroute::AdminEndpointHandler endpoint(admin);
    signalroute::AdminHttpHandler handler(endpoint);

    const auto response = handler.handle({"POST", "/health", "application/json"});

    assert(response.status_code == 405);
    assert(response.reason_phrase == "Method Not Allowed");
    assert(has_header(response, "Allow", "GET, HEAD"));
    assert(response.body == R"({"error":"method not allowed"})");
}

void test_serialize_http_response_writes_status_headers_and_body() {
    signalroute::AdminHttpResponse response;
    response.status_code = 200;
    response.reason_phrase = "OK";
    response.content_type = "text/plain";
    response.body = "ok";
    response.headers = {{"Content-Type", "text/plain"}, {"Content-Length", "2"}};

    const auto wire = signalroute::serialize_http_response(response);

    assert(wire.find("HTTP/1.1 200 OK\r\n") == 0);
    assert(wire.find("Content-Type: text/plain\r\n") != std::string::npos);
    assert(wire.rfind("\r\n\r\nok") == wire.size() - 6);
}

void test_reason_phrases_include_socket_hardening_statuses() {
    assert(signalroute::reason_phrase_for_status(408) == "Request Timeout");
    assert(signalroute::reason_phrase_for_status(413) == "Payload Too Large");
}

int main() {
    std::cout << "test_admin_http_handler:\n";
    test_health_route_returns_json_200();
    test_ready_alias_maps_to_readiness_and_unhealthy_returns_503();
    test_metrics_route_returns_prometheus_text();
    test_head_returns_headers_without_body();
    test_unknown_path_returns_404();
    test_unsupported_method_returns_405_with_allow_header();
    test_serialize_http_response_writes_status_headers_and_body();
    test_reason_phrases_include_socket_hardening_statuses();
    std::cout << "All admin HTTP handler tests passed.\n";
    return 0;
}
