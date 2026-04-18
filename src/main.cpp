#include "common/config/config.h"
#include "common/metrics/metrics.h"
#include "gateway/http_server.h"
#include "processor/processor_service.h"
#include "geofence/geofence_engine.h"
#include "matching/matching_service.h"

#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>

std::atomic<bool> g_stop_flag{false};

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        g_stop_flag.store(true);
    }
}

int main(int argc, char** argv) {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::string config_path = "config.toml";
    if (argc > 1) {
        config_path = argv[1];
    }

    std::cout << "[SignalRoute] Starting. Config: " << config_path << "\n";

    // 1. Load config
    signalroute::Config config;
    try {
        config = signalroute::Config::load(config_path);
    } catch (const std::exception& e) {
        std::cerr << "Failed to load config: " << e.what() << "\n";
        return 1;
    }

    // 2. Initialize Metrics
    signalroute::Metrics::instance().init(config.observability);

    // 3. Initialize Services
    signalroute::HttpServer gateway;
    signalroute::ProcessorService processor;
    signalroute::GeofenceEngine geofence;
    signalroute::MatchingService matching;

    try {
        // Start dependencies first
        processor.start(config);
        
        if (config.geofence.enabled) {
            geofence.start(config);
        }
        
        if (config.matching.enabled) {
            matching.start(config);
        }

        // Start Gateway last
        gateway.start(config);

        std::cout << "[SignalRoute] All services started. Press Ctrl+C to stop.\n";

        // Wait for stop signal
        while (!g_stop_flag.load()) {
            // Check health
            if (!gateway.is_healthy() || !processor.is_healthy()) {
                std::cerr << "[SignalRoute] Critical service unhealthy! Initiating shutdown.\n";
                break;
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        std::cout << "[SignalRoute] Shutting down...\n";

        // Stop in reverse order
        gateway.stop();
        if (config.matching.enabled) matching.stop();
        if (config.geofence.enabled) geofence.stop();
        processor.stop();

        std::cout << "[SignalRoute] Shutdown complete.\n";

    } catch (const std::exception& e) {
        std::cerr << "[SignalRoute] Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
