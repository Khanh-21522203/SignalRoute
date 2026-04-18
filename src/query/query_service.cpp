#include "query_service.h"
#include <iostream>

namespace signalroute {

QueryService::QueryService() = default;
QueryService::~QueryService() { if (running_) stop(); }

void QueryService::start(const Config& /*config*/) {
    std::cout << "[QueryService] Starting...\n";
    running_ = true;
    std::cout << "[QueryService] Started.\n";
}

void QueryService::stop() {
    std::cout << "[QueryService] Stopping...\n";
    running_ = false;
    std::cout << "[QueryService] Stopped.\n";
}

bool QueryService::is_healthy() const { return running_; }

} // namespace signalroute
