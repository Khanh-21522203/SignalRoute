#include "gateway_grpc_adapter.h"

#if SIGNALROUTE_HAS_GRPC

#include "common/proto/generated_conversions.h"

#include <cstddef>
#include <sstream>
#include <vector>

namespace signalroute::transport::grpc {
namespace {

std::string join_errors(const std::vector<std::string>& errors) {
    std::ostringstream out;
    for (std::size_t i = 0; i < errors.size(); ++i) {
        if (i > 0) {
            out << "; ";
        }
        out << errors[i];
    }
    return out.str();
}

} // namespace

GatewayGrpcService::GatewayGrpcService(GatewayService& gateway) : gateway_(gateway) {}

::grpc::Status GatewayGrpcService::IngestSingle(
    ::grpc::ServerContext* /*context*/,
    const signalroute::v1::IngestSingleRequest* request,
    signalroute::v1::IngestSingleResponse* response) {
    const auto event = proto_boundary::location_event_from_generated(request->event());
    if (event.is_err()) {
        response->set_ok(false);
        response->set_reason(event.error());
        return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, event.error());
    }

    auto result = gateway_.handle_ingest_one(IngestOneRequest{event.value()});
    response->set_ok(result.ok());
    if (!result.ok()) {
        const auto reason = join_errors(result.errors);
        response->set_reason(reason);
        return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, reason);
    }
    return ::grpc::Status::OK;
}

::grpc::Status GatewayGrpcService::IngestBatch(
    ::grpc::ServerContext* /*context*/,
    const signalroute::v1::IngestBatchRequest* request,
    signalroute::v1::IngestBatchResponse* response) {
    std::vector<LocationEvent> events;
    events.reserve(static_cast<std::size_t>(request->batch().events_size()));

    for (const auto& message : request->batch().events()) {
        auto event = proto_boundary::location_event_from_generated(message);
        if (event.is_err()) {
            response->set_accepted(0);
            response->set_rejected(request->batch().events_size());
            response->set_reason(event.error());
            return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, event.error());
        }
        events.push_back(event.value());
    }

    auto result = gateway_.handle_ingest_batch(IngestBatchRequest{events});
    response->set_accepted(result.accepted_count);
    response->set_rejected(result.rejected_count);
    if (!result.ok()) {
        response->set_reason(join_errors(result.errors));
    }
    return ::grpc::Status::OK;
}

} // namespace signalroute::transport::grpc

#endif
