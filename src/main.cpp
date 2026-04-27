#include "common/config/config.h"
#include "common/metrics/metrics.h"
#include "runtime/runtime_application.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <exception>
#include <iostream>
#include <string>
#include <thread>

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

    signalroute::Config config;
    try {
        config = signalroute::Config::load(config_path);
        if (!role_override.empty()) {
            config.server.role = role_override;
            config.validate();
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to load config: " << e.what() << "\n";
        return 1;
    }

    signalroute::Metrics::instance().initialize(
        config.observability.metrics_addr,
        config.observability.metrics_port,
        config.observability.metrics_path);

    signalroute::RuntimeApplication app;
    try {
        app.start(config);
        std::cout << "[SignalRoute] Role '" << config.server.role
                  << "' started. Press Ctrl+C to stop.\n";

        while (!g_stop_flag.load()) {
            if (!app.is_healthy()) {
                std::cerr << "[SignalRoute] Runtime unhealthy! Initiating shutdown.\n";
                break;
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        std::cout << "[SignalRoute] Shutting down...\n";
        app.stop();
        std::cout << "[SignalRoute] Shutdown complete.\n";
    } catch (const std::exception& e) {
        std::cerr << "[SignalRoute] Fatal error: " << e.what() << "\n";
        if (app.is_running()) {
            app.stop();
        }
        return 1;
    }

    return 0;
}
