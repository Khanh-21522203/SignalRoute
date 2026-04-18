#include "processor_service.h"
#include <iostream>

namespace signalroute {

ProcessorService::ProcessorService() = default;
ProcessorService::~ProcessorService() { if (running_) stop(); }

void ProcessorService::start(const Config& /*config*/) {
    std::cout << "[ProcessorService] Starting...\n";
    running_ = true;
    // TODO: Initialize all components and start processing loop
    std::cout << "[ProcessorService] Started.\n";
}

void ProcessorService::stop() {
    std::cout << "[ProcessorService] Stopping...\n";
    running_ = false;
    // TODO: Drain in-flight, flush history writer, commit offsets
    std::cout << "[ProcessorService] Stopped.\n";
}

bool ProcessorService::is_healthy() const { return running_; }

} // namespace signalroute
