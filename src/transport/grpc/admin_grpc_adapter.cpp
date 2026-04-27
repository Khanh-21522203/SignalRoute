#include "admin_grpc_adapter.h"

#if SIGNALROUTE_HAS_GRPC

#include <string>

namespace signalroute::transport::grpc {
namespace {

bool component_healthy(const HealthResponse& health, const std::string& name) {
    for (const auto& component : health.components) {
        if (component.name == name) {
            return component.healthy;
        }
    }
    return false;
}

} // namespace

AdminGrpcService::AdminGrpcService(signalroute::AdminService& admin) : admin_(admin) {}

::grpc::Status AdminGrpcService::Health(
    ::grpc::ServerContext* /*context*/,
    const signalroute::v1::HealthRequest* /*request*/,
    signalroute::v1::HealthResponse* response) {
    const auto health = admin_.health();
    response->set_healthy(health.healthy);
    response->set_role(health.role);
    response->set_version(health.version);
    response->set_uptime_s(health.uptime_s);
    response->set_kafka_connected(
        component_healthy(health, "kafka") || component_healthy(health, "event_bus"));
    response->set_redis_connected(component_healthy(health, "redis"));
    response->set_postgis_connected(component_healthy(health, "postgis"));
    return ::grpc::Status::OK;
}

::grpc::Status AdminGrpcService::Metrics(
    ::grpc::ServerContext* /*context*/,
    const signalroute::v1::MetricsRequest* /*request*/,
    signalroute::v1::MetricsResponse* response) {
    response->set_metrics_text(admin_.metrics().metrics_text);
    return ::grpc::Status::OK;
}

} // namespace signalroute::transport::grpc

#endif
