#pragma once

/**
 * SignalRoute — Configuration
 *
 * Loads the TOML configuration file and provides typed access to all
 * configuration sections. Each struct maps to a [section] in the TOML file.
 *
 * Dependencies: none. Config::load supports the TOML subset used by
 * config/signalroute.toml: sections, scalar string/bool/int/double values,
 * comments, and default fallback for omitted keys.
 *
 * Usage:
 *   auto config = signalroute::Config::load("config/signalroute.toml");
 *   int port = config.server.grpc_port;
 */

#include <string>
#include <optional>
#include <vector>

namespace signalroute {

struct ServerConfig {
    std::string role        = "standalone";
    std::string listen_addr = "0.0.0.0";
    int         grpc_port   = 9090;
    int         udp_port    = 9091;
    std::string tls_cert;
    std::string tls_key;
};

struct KafkaConfig {
    std::string brokers         = "localhost:9092";
    std::string ingest_topic    = "tm.location.events";
    std::string geofence_topic  = "tm.geofence.events";
    std::string dlq_topic       = "tm.location.dlq";
    std::string consumer_group  = "signalroute-processor";
    int         num_partitions  = 16;
    int         batch_size_bytes = 65536;
    int         linger_ms       = 5;
};

struct RedisConfig {
    std::string addrs              = "localhost:6379";
    int         pool_size          = 32;
    int         connect_timeout_ms = 1000;
    int         read_timeout_ms    = 500;
    std::string key_prefix         = "sr";
    int         device_ttl_s       = 3600;
};

struct PostGISConfig {
    std::string dsn;
    int         pool_size              = 16;
    int         write_batch_size       = 500;
    int         write_flush_interval_ms = 500;
    int         query_timeout_ms       = 5000;
};

struct ProcessorConfig {
    int  dedup_ttl_s                = 300;
    int  dedup_max_entries          = 500000;
    bool sequence_guard_enabled     = true;
    int  out_of_order_tolerance_s   = 60;
    int  history_batch_size         = 500;
    int  history_flush_interval_ms  = 500;
    int  history_buffer_max_rows    = 10000;
    int  redis_max_retries          = 3;
    int  redis_backoff_max_ms       = 5000;
};

struct SpatialConfig {
    int    h3_resolution       = 7;
    int    nearby_max_results  = 1000;
    double nearby_max_radius_m = 50000.0;
    int    h3_cache_size_mb    = 64;
};

struct GeofenceConfig {
    bool eval_enabled       = true;
    int  dwell_threshold_s  = 300;
    int  max_fences         = 10000;
    int  reload_interval_s  = 60;
};

struct GatewayConfig {
    int max_batch_events              = 1000;
    int rate_limit_rps_per_device     = 100;
    int timestamp_skew_tolerance_s    = 30;
    int queue_full_timeout_ms         = 500;
    bool auth_required                = false;
    std::string api_key;
    int max_in_flight_requests        = 0; // 0 = unlimited
};

struct MatchingConfig {
    std::string strategy_name  = "default";
    std::string request_topic  = "sr.match.requests";
    std::string result_topic   = "sr.match.results";
    int         request_ttl_ms = 5000;
    // TODO: Strategy-specific config subtree (parsed as raw TOML table)
};

struct ThreadConfig {
    int io_threads           = 0;  // 0 = auto
    int processor_threads    = 0;  // 0 = auto
    int geofence_eval_threads = 4;
    int blocking_pool_size   = 16;
};

struct ObservabilityConfig {
    std::string metrics_addr = "0.0.0.0";
    int         metrics_port = 9100;
    std::string metrics_path = "/metrics";
    std::string log_level    = "info";
    bool        admin_http_enabled = true;
    bool        admin_socket_enabled = false;
    std::string admin_socket_addr = "127.0.0.1";
    int         admin_socket_port = 9101;
    int         admin_socket_backlog = 16;
    std::string health_path = "/health";
    std::string readiness_path = "/ready";
    bool        require_kafka_readiness = false;
    bool        require_redis_readiness = false;
    bool        require_postgis_readiness = false;
    bool        require_h3_readiness = false;
};

/**
 * Top-level configuration.
 * Aggregates all configuration sections.
 */
class Config {
public:
    /**
     * Load configuration from a TOML file.
     *
     * @param path Filesystem path to the TOML configuration file.
     * @return Populated Config instance.
     * @throws std::runtime_error if the file cannot be read or parsed.
     *
     * Each [section] in the TOML file maps to a member struct.
     * Missing keys fall back to the default values defined in each struct.
     */
    static Config load(const std::string& path);
    void validate() const;

    ServerConfig        server;
    KafkaConfig         kafka;
    RedisConfig         redis;
    PostGISConfig       postgis;
    ProcessorConfig     processor;
    SpatialConfig       spatial;
    GeofenceConfig      geofence;
    GatewayConfig       gateway;
    MatchingConfig      matching;
    ThreadConfig        threads;
    ObservabilityConfig observability;
};

} // namespace signalroute
