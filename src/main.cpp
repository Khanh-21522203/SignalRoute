#include "common/config/config.h"
#include "common/logging/structured_log.h"
#include "common/metrics/metrics.h"
#include "runtime/runtime_application.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <exception>
#include <iostream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

std::atomic<bool> g_stop_flag{false};

void log_runtime_event(
    const std::string& level,
    const std::string& event,
    const std::string& message,
    std::vector<signalroute::LogField> fields = {}) {
    signalroute::write_logfmt(
        level == "error" ? std::cerr : std::cout,
        signalroute::make_log_event(level, "signalroute", event, message, std::move(fields)));
}

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

    log_runtime_event(
        "info",
        "runtime.starting",
        "starting SignalRoute process",
        {{"config_path", config_path}});

    signalroute::Config config;
    try {
        config = signalroute::Config::load(config_path);
        if (!role_override.empty()) {
            config.server.role = role_override;
            config.validate();
        }
    } catch (const std::exception& e) {
        log_runtime_event(
            "error",
            "runtime.config_failed",
            "failed to load config",
            {{"config_path", config_path}, {"error", e.what()}});
        return 1;
    }

    signalroute::Metrics::instance().initialize(
        config.observability.metrics_addr,
        config.observability.metrics_port,
        config.observability.metrics_path);

    signalroute::RuntimeApplication app;
    try {
        app.start(config);
        log_runtime_event(
            "info",
            "runtime.started",
            "runtime started",
            {{"role", config.server.role}});
        if (app.admin_socket_enabled()) {
            log_runtime_event(
                "info",
                "admin.socket_started",
                "admin socket started",
                {
                    {"addr", config.observability.admin_socket_addr},
                    {"port", std::to_string(app.admin_socket_bound_port())},
                });
        }

        while (!g_stop_flag.load()) {
            if (!app.is_healthy()) {
                log_runtime_event(
                    "error",
                    "runtime.unhealthy",
                    "runtime unhealthy; initiating shutdown",
                    {{"role", config.server.role}});
                break;
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        log_runtime_event(
            "info",
            "runtime.stopping",
            "runtime stopping",
            {{"role", config.server.role}});
        app.stop();
        log_runtime_event(
            "info",
            "runtime.stopped",
            "runtime stopped",
            {{"role", config.server.role}});
    } catch (const std::exception& e) {
        log_runtime_event(
            "error",
            "runtime.fatal",
            "fatal runtime error",
            {{"role", config.server.role}, {"error", e.what()}});
        if (app.is_running()) {
            app.stop();
        }
        return 1;
    }

    return 0;
}
