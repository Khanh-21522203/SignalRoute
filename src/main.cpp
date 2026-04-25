#include "common/config/config.h"
#include "common/metrics/metrics.h"
#include "gateway/gateway_service.h"
#include "processor/processor_service.h"
#include "query/query_service.h"
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

    std::string config_path = "config/signalroute.toml";
    std::string role_override;
    if (argc > 1) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg.rfind("--role=", 0) == 0) {
                role_override = arg.substr(7);
            } else if (arg.rfind("--config=", 0) == 0) {
                config_path = arg.substr(9);
            } else {
                config_path = arg;
            }
        }
    }

    std::cout << "[SignalRoute] Starting. Config: " << config_path << "\n";

    // 1. Load config
    signalroute::Config config;
    try {
        config = signalroute::Config::load(config_path);
        if (!role_override.empty()) {
            config.server.role = role_override;
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to load config: " << e.what() << "\n";
        return 1;
    }

    // 2. Initialize Metrics
    signalroute::Metrics::instance().initialize(
        config.observability.metrics_addr,
        config.observability.metrics_port,
        config.observability.metrics_path);

    // 3. Initialize Services
    signalroute::GatewayService gateway;
    signalroute::ProcessorService processor;
    signalroute::QueryService query;
    signalroute::GeofenceEngine geofence;
    signalroute::MatchingService matching;

    try {
        const auto& role = config.server.role;

        if (role == "standalone" || role == "processor") {
            processor.start(config);
        }

        if (role == "standalone" || role == "query") {
            query.start(config);
        }

        if ((role == "standalone" || role == "geofence") && config.geofence.eval_enabled) {
            geofence.start(config);
        }

        if (role == "standalone" || role == "matcher") {
            matching.start(config);
        }

        if (role == "standalone" || role == "gateway") {
            gateway.start(config);
        }

        std::cout << "[SignalRoute] Role '" << role << "' started. Press Ctrl+C to stop.\n";

        // Wait for stop signal
        while (!g_stop_flag.load()) {
            // Check health
            if ((role == "standalone" || role == "gateway") && !gateway.is_healthy()) {
                std::cerr << "[SignalRoute] Gateway unhealthy! Initiating shutdown.\n";
                break;
            }
            if ((role == "standalone" || role == "processor") && !processor.is_healthy()) {
                std::cerr << "[SignalRoute] Processor unhealthy! Initiating shutdown.\n";
                break;
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        std::cout << "[SignalRoute] Shutting down...\n";

        // Stop in reverse order
        if (role == "standalone" || role == "gateway") gateway.stop();
        if (role == "standalone" || role == "matcher") matching.stop();
        if ((role == "standalone" || role == "geofence") && config.geofence.eval_enabled) geofence.stop();
        if (role == "standalone" || role == "query") query.stop();
        if (role == "standalone" || role == "processor") processor.stop();

        std::cout << "[SignalRoute] Shutdown complete.\n";

    } catch (const std::exception& e) {
        std::cerr << "[SignalRoute] Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
