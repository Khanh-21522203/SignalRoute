#pragma once

#include "query/query_service.h"

#if SIGNALROUTE_HAS_GRPC
#include "signalroute/query.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#endif

namespace signalroute::transport::grpc {

#if SIGNALROUTE_HAS_GRPC

class QueryGrpcService final : public signalroute::v1::QueryService::Service {
public:
    explicit QueryGrpcService(QueryService& query);

    ::grpc::Status GetLatestLocation(
        ::grpc::ServerContext* context,
        const signalroute::v1::GetLatestRequest* request,
        signalroute::v1::DeviceStateResponse* response) override;

    ::grpc::Status NearbyDevices(
        ::grpc::ServerContext* context,
        const signalroute::v1::NearbyRequest* request,
        signalroute::v1::NearbyResponse* response) override;

    ::grpc::Status GetTrip(
        ::grpc::ServerContext* context,
        const signalroute::v1::TripRequest* request,
        signalroute::v1::TripResponse* response) override;

private:
    QueryService& query_;
};

#endif

} // namespace signalroute::transport::grpc
