#include "dependency_health.h"

#include "../clients/postgres_client.h"
#include "../clients/redis_client.h"
#include "../kafka/kafka_consumer.h"
#include "../kafka/kafka_producer.h"
#include "../spatial/h3_index.h"

#include <exception>
#include <string>
#include <utility>

#ifndef SIGNALROUTE_HAS_KAFKA
#define SIGNALROUTE_HAS_KAFKA 0
#endif

#ifndef SIGNALROUTE_HAS_REDIS
#define SIGNALROUTE_HAS_REDIS 0
#endif

#ifndef SIGNALROUTE_HAS_POSTGIS
#define SIGNALROUTE_HAS_POSTGIS 0
#endif

#ifndef SIGNALROUTE_HAS_H3
#define SIGNALROUTE_HAS_H3 0
#endif

namespace signalroute {

void DependencyHealthRegistry::register_source(std::string name, Probe probe) {
    probes_[std::move(name)] = std::move(probe);
}

DependencyHealthSnapshot DependencyHealthRegistry::check(const std::string& name) const {
    const auto it = probes_.find(name);
    if (it == probes_.end() || !it->second) {
        return DependencyHealthSnapshot{name, false, "dependency health source not registered"};
    }
    auto snapshot = it->second();
    if (snapshot.name.empty()) {
        snapshot.name = name;
    }
    return snapshot;
}

ComponentHealth DependencyHealthRegistry::readiness_component(const std::string& name) const {
    const auto snapshot = check(name);
    return ComponentHealth{
        snapshot.name,
        snapshot.healthy,
        true,
        snapshot.detail,
    };
}

std::size_t DependencyHealthRegistry::source_count() const {
    return probes_.size();
}

DependencyHealthSnapshot build_flag_dependency_health(std::string name, bool enabled) {
    return DependencyHealthSnapshot{
        std::move(name),
        enabled,
        enabled ? "production adapter enabled" : "production adapter required but not enabled in this build",
    };
}

DependencyHealthSnapshot kafka_producer_health(const KafkaProducer& producer) {
    const bool connected = producer.is_connected();
    return DependencyHealthSnapshot{
        "kafka",
        connected,
        connected ? "kafka producer connected" : "kafka producer disconnected",
    };
}

DependencyHealthSnapshot kafka_consumer_health(const KafkaConsumer& consumer) {
    const bool connected = consumer.is_connected();
    return DependencyHealthSnapshot{
        "kafka",
        connected,
        connected ? "kafka consumer connected" : "kafka consumer disconnected",
    };
}

DependencyHealthSnapshot redis_health(RedisClient& redis) {
    const bool reachable = redis.ping();
    return DependencyHealthSnapshot{
        "redis",
        reachable,
        reachable ? "redis ping ok" : "redis ping failed",
    };
}

DependencyHealthSnapshot postgis_health(PostgresClient& postgis) {
    const bool reachable = postgis.ping();
    return DependencyHealthSnapshot{
        "postgis",
        reachable,
        reachable ? "postgis ping ok" : "postgis ping failed",
    };
}

DependencyHealthSnapshot h3_health(const H3Index& h3) {
    try {
        const double edge_m = h3.avg_edge_length_m();
        return DependencyHealthSnapshot{
            "h3",
            edge_m > 0.0,
            edge_m > 0.0 ? "h3 index ready" : "h3 index returned invalid edge length",
        };
    } catch (const std::exception& ex) {
        return DependencyHealthSnapshot{"h3", false, std::string("h3 health check failed: ") + ex.what()};
    } catch (...) {
        return DependencyHealthSnapshot{"h3", false, "h3 health check failed"};
    }
}

DependencyHealthRegistry default_dependency_health_registry() {
    DependencyHealthRegistry registry;
    registry.register_source("kafka", [] {
        return build_flag_dependency_health("kafka", SIGNALROUTE_HAS_KAFKA == 1);
    });
    registry.register_source("redis", [] {
        return build_flag_dependency_health("redis", SIGNALROUTE_HAS_REDIS == 1);
    });
    registry.register_source("postgis", [] {
        return build_flag_dependency_health("postgis", SIGNALROUTE_HAS_POSTGIS == 1);
    });
    registry.register_source("h3", [] {
        return build_flag_dependency_health("h3", SIGNALROUTE_HAS_H3 == 1);
    });
    return registry;
}

} // namespace signalroute
