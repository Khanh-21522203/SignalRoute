#pragma once

#include "common/admin/admin_service.h"

#if SIGNALROUTE_HAS_GRPC
#include "signalroute/admin.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#endif

namespace signalroute::transport::grpc {

#if SIGNALROUTE_HAS_GRPC

class AdminGrpcService final : public signalroute::v1::AdminService::Service {
public:
    explicit AdminGrpcService(signalroute::AdminService& admin);

    ::grpc::Status Health(
        ::grpc::ServerContext* context,
        const signalroute::v1::HealthRequest* request,
        signalroute::v1::HealthResponse* response) override;

    ::grpc::Status Metrics(
        ::grpc::ServerContext* context,
        const signalroute::v1::MetricsRequest* request,
        signalroute::v1::MetricsResponse* response) override;

private:
    signalroute::AdminService& admin_;
};

#endif

} // namespace signalroute::transport::grpc
