#include "common/config/config.h"
#include "common/events/event_bus.h"
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
    signalroute::EventBus event_bus;

    try {
        const auto& role = config.server.role;
        const auto wants_gateway = role == "standalone" || role == "gateway";
        const auto wants_processor = role == "standalone" || role == "processor";
        const auto wants_query = role == "standalone" || role == "query";
        const auto wants_geofence = role == "standalone" || role == "geofence";
        const auto wants_matching = role == "standalone" || role == "matcher" || role == "matching";

        if (wants_processor) {
            processor.start(config, event_bus);
        }

        if (wants_query) {
            query.start(config);
        }

        if (wants_geofence && config.geofence.eval_enabled) {
            geofence.start(config, event_bus);
        }

        if (wants_matching) {
            matching.start(config);
        }

        if (wants_gateway) {
            gateway.start(config, event_bus);
        }

        std::cout << "[SignalRoute] Role '" << role << "' started. Press Ctrl+C to stop.\n";

        // Wait for stop signal
        while (!g_stop_flag.load()) {
            // Check health
            if (wants_gateway && !gateway.is_healthy()) {
                std::cerr << "[SignalRoute] Gateway unhealthy! Initiating shutdown.\n";
                break;
            }
            if (wants_processor && !processor.is_healthy()) {
                std::cerr << "[SignalRoute] Processor unhealthy! Initiating shutdown.\n";
                break;
            }
            if (wants_query && !query.is_healthy()) {
                std::cerr << "[SignalRoute] Query service unhealthy! Initiating shutdown.\n";
                break;
            }
            if (wants_geofence && config.geofence.eval_enabled && !geofence.is_healthy()) {
                std::cerr << "[SignalRoute] Geofence engine unhealthy! Initiating shutdown.\n";
                break;
            }
            if (wants_matching && !matching.is_healthy()) {
                std::cerr << "[SignalRoute] Matching service unhealthy! Initiating shutdown.\n";
                break;
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        std::cout << "[SignalRoute] Shutting down...\n";

        // Stop in reverse order
        if (wants_gateway) gateway.stop();
        if (wants_matching) matching.stop();
        if (wants_geofence && config.geofence.eval_enabled) geofence.stop();
        if (wants_query) query.stop();
        if (wants_processor) processor.stop();

        std::cout << "[SignalRoute] Shutdown complete.\n";

    } catch (const std::exception& e) {
        std::cerr << "[SignalRoute] Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
