#include "query_grpc_adapter.h"

#if SIGNALROUTE_HAS_GRPC

#include "common/proto/generated_conversions.h"

#include <string>

namespace signalroute::transport::grpc {
namespace {

void copy_trip_point(const LocationEvent& event, signalroute::v1::TripPoint* point) {
    point->set_device_id(event.device_id);
    point->set_lat(event.lat);
    point->set_lon(event.lon);
    point->set_altitude_m(event.altitude_m);
    point->set_accuracy_m(event.accuracy_m);
    point->set_speed_ms(event.speed_ms);
    point->set_heading_deg(event.heading_deg);
    point->set_event_time(event.timestamp_ms);
    point->set_recv_time(event.server_recv_ms);
    point->set_seq(event.seq);
}

::grpc::Status invalid_request(const std::string& error) {
    return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, error);
}

} // namespace

QueryGrpcService::QueryGrpcService(QueryService& query) : query_(query) {}

::grpc::Status QueryGrpcService::GetLatestLocation(
    ::grpc::ServerContext* /*context*/,
    const signalroute::v1::GetLatestRequest* request,
    signalroute::v1::DeviceStateResponse* response) {
    const auto result = query_.handle_latest(LatestLocationRequest{request->device_id()});
    if (!result.ok) {
        return invalid_request(result.error);
    }
    if (!result.found) {
        return ::grpc::Status(::grpc::StatusCode::NOT_FOUND, "device state not found");
    }

    *response = proto_boundary::to_generated_device_state_response(result.state);
    return ::grpc::Status::OK;
}

::grpc::Status QueryGrpcService::NearbyDevices(
    ::grpc::ServerContext* /*context*/,
    const signalroute::v1::NearbyRequest* request,
    signalroute::v1::NearbyResponse* response) {
    const auto result = query_.handle_nearby(NearbyDevicesRequest{
        request->lat(),
        request->lon(),
        request->radius_m(),
        request->limit(),
        request->last_seen_s(),
    });
    if (!result.ok) {
        return invalid_request(result.error);
    }

    response->set_total_candidates(result.result.total_candidates);
    response->set_total_in_radius(result.result.total_in_radius);
    for (const auto& [state, distance_m] : result.result.devices) {
        auto* device = response->add_devices();
        device->set_device_id(state.device_id);
        device->set_lat(state.lat);
        device->set_lon(state.lon);
        device->set_distance_m(distance_m);
        device->set_updated_at(state.updated_at);
        device->set_speed_ms(state.speed_ms);
        device->set_heading_deg(state.heading_deg);
    }
    return ::grpc::Status::OK;
}

::grpc::Status QueryGrpcService::GetTrip(
    ::grpc::ServerContext* /*context*/,
    const signalroute::v1::TripRequest* request,
    signalroute::v1::TripResponse* response) {
    TripQueryResponse result;
    if (request->has_circle()) {
        result = query_.handle_trip_spatial(SpatialTripQueryRequest{
            request->device_id(),
            request->from_ts(),
            request->to_ts(),
            request->circle().lat(),
            request->circle().lon(),
            request->circle().radius_m(),
            request->sample_interval_s(),
            request->limit(),
        });
    } else if (request->has_bbox()) {
        return ::grpc::Status(
            ::grpc::StatusCode::UNIMPLEMENTED,
            "bbox trip filter is not implemented in the fallback query handler");
    } else {
        result = query_.handle_trip(TripQueryRequest{
            request->device_id(),
            request->from_ts(),
            request->to_ts(),
            request->sample_interval_s(),
            request->limit(),
        });
    }

    if (!result.ok) {
        return invalid_request(result.error);
    }

    response->set_total_points(static_cast<int>(result.events.size()));
    for (const auto& event : result.events) {
        copy_trip_point(event, response->add_points());
    }
    return ::grpc::Status::OK;
}

} // namespace signalroute::transport::grpc

#endif
