#include "gateway_service.h"
#include <iostream>

namespace signalroute {

GatewayService::GatewayService() = default;
GatewayService::~GatewayService() { if (running_) stop(); }

void GatewayService::start(const Config& /*config*/) {
    // TODO: Initialize all gateway components
    std::cout << "[GatewayService] Starting...\n";
    running_ = true;
    // TODO: Start gRPC and UDP servers
    std::cout << "[GatewayService] Started.\n";
}

void GatewayService::stop() {
    std::cout << "[GatewayService] Stopping...\n";
    running_ = false;
    // TODO: Shutdown sequence
    std::cout << "[GatewayService] Stopped.\n";
}

bool GatewayService::is_healthy() const {
    return running_;
}

} // namespace signalroute
