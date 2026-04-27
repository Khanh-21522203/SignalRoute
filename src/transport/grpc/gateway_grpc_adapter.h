#pragma once

#include "gateway/gateway_service.h"

#if SIGNALROUTE_HAS_GRPC
#include "signalroute/location.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#endif

namespace signalroute::transport::grpc {

#if SIGNALROUTE_HAS_GRPC

class GatewayGrpcService final : public signalroute::v1::IngestService::Service {
public:
    explicit GatewayGrpcService(GatewayService& gateway);

    ::grpc::Status IngestSingle(
        ::grpc::ServerContext* context,
        const signalroute::v1::IngestSingleRequest* request,
        signalroute::v1::IngestSingleResponse* response) override;

    ::grpc::Status IngestBatch(
        ::grpc::ServerContext* context,
        const signalroute::v1::IngestBatchRequest* request,
        signalroute::v1::IngestBatchResponse* response) override;

private:
    GatewayService& gateway_;
};

#endif

} // namespace signalroute::transport::grpc
