#include "config.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>

namespace signalroute {
namespace {

using SectionMap = std::map<std::string, std::map<std::string, std::string>>;

std::string trim(const std::string& value) {
    std::size_t begin = 0;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin]))) {
        ++begin;
    }

    std::size_t end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }

    return value.substr(begin, end - begin);
}

std::string strip_comment(const std::string& line) {
    bool in_string = false;
    bool escaped = false;

    for (std::size_t i = 0; i < line.size(); ++i) {
        const char ch = line[i];

        if (escaped) {
            escaped = false;
            continue;
        }
        if (in_string && ch == '\\') {
            escaped = true;
            continue;
        }
        if (ch == '"') {
            in_string = !in_string;
            continue;
        }
        if (!in_string && ch == '#') {
            return line.substr(0, i);
        }
    }

    return line;
}

[[noreturn]] void parse_error(const std::string& path, int line_no, const std::string& message) {
    std::ostringstream out;
    out << "Config parse error in " << path << ':' << line_no << ": " << message;
    throw std::runtime_error(out.str());
}

std::string unquote(const std::string& raw, const std::string& path, int line_no) {
    if (raw.size() < 2 || raw.front() != '"' || raw.back() != '"') {
        parse_error(path, line_no, "expected quoted string value");
    }

    std::string out;
    out.reserve(raw.size() - 2);
    bool escaped = false;
    for (std::size_t i = 1; i + 1 < raw.size(); ++i) {
        const char ch = raw[i];
        if (escaped) {
            switch (ch) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                default:
                    parse_error(path, line_no, "unsupported string escape");
            }
            escaped = false;
            continue;
        }
        if (ch == '\\') {
            escaped = true;
            continue;
        }
        out.push_back(ch);
    }
    if (escaped) {
        parse_error(path, line_no, "unterminated string escape");
    }
    return out;
}

SectionMap parse_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Config file not found or unreadable: " + path);
    }

    SectionMap sections;
    std::string current_section;
    std::string line;
    int line_no = 0;

    while (std::getline(file, line)) {
        ++line_no;
        line = trim(strip_comment(line));
        if (line.empty()) {
            continue;
        }

        if (line.front() == '[') {
            if (line.back() != ']' || line.size() < 3) {
                parse_error(path, line_no, "invalid section header");
            }
            current_section = trim(line.substr(1, line.size() - 2));
            if (current_section.empty()) {
                parse_error(path, line_no, "empty section name");
            }
            sections[current_section];
            continue;
        }

        const std::size_t eq = line.find('=');
        if (eq == std::string::npos) {
            parse_error(path, line_no, "expected key = value");
        }
        if (current_section.empty()) {
            parse_error(path, line_no, "key appears before any section");
        }

        const std::string key = trim(line.substr(0, eq));
        const std::string raw_value = trim(line.substr(eq + 1));
        if (key.empty()) {
            parse_error(path, line_no, "empty key");
        }
        if (raw_value.empty()) {
            parse_error(path, line_no, "empty value");
        }

        sections[current_section][key] = raw_value;
    }

    return sections;
}

const std::string* raw_value(const SectionMap& sections,
                             const std::string& section,
                             const std::string& key) {
    auto section_it = sections.find(section);
    if (section_it == sections.end()) {
        return nullptr;
    }

    auto value_it = section_it->second.find(key);
    if (value_it == section_it->second.end()) {
        return nullptr;
    }

    return &value_it->second;
}

std::string get_string(const SectionMap& sections,
                       const std::string& section,
                       const std::string& key,
                       const std::string& fallback,
                       const std::string& path) {
    const std::string* value = raw_value(sections, section, key);
    if (!value) {
        return fallback;
    }
    return unquote(*value, path, 0);
}

bool get_bool(const SectionMap& sections,
              const std::string& section,
              const std::string& key,
              bool fallback,
              const std::string& path) {
    const std::string* value = raw_value(sections, section, key);
    if (!value) {
        return fallback;
    }
    if (*value == "true") {
        return true;
    }
    if (*value == "false") {
        return false;
    }
    throw std::runtime_error("Config parse error in " + path + ": expected boolean for " + section + "." + key);
}

int get_int(const SectionMap& sections,
            const std::string& section,
            const std::string& key,
            int fallback,
            const std::string& path) {
    const std::string* value = raw_value(sections, section, key);
    if (!value) {
        return fallback;
    }

    try {
        std::size_t pos = 0;
        const long parsed = std::stol(*value, &pos, 10);
        if (pos != value->size()) {
            throw std::invalid_argument("trailing characters");
        }
        if (parsed < static_cast<long>(std::numeric_limits<int>::min()) ||
            parsed > static_cast<long>(std::numeric_limits<int>::max())) {
            throw std::out_of_range("integer out of range");
        }
        return static_cast<int>(parsed);
    } catch (const std::exception&) {
        throw std::runtime_error("Config parse error in " + path + ": expected integer for " + section + "." + key);
    }
}

double get_double(const SectionMap& sections,
                  const std::string& section,
                  const std::string& key,
                  double fallback,
                  const std::string& path) {
    const std::string* value = raw_value(sections, section, key);
    if (!value) {
        return fallback;
    }

    try {
        std::size_t pos = 0;
        const double parsed = std::stod(*value, &pos);
        if (pos != value->size()) {
            throw std::invalid_argument("trailing characters");
        }
        return parsed;
    } catch (const std::exception&) {
        throw std::runtime_error("Config parse error in " + path + ": expected number for " + section + "." + key);
    }
}

void require_non_empty(const std::string& value, const std::string& name) {
    if (value.empty()) {
        throw std::runtime_error("Invalid config: " + name + " must not be empty");
    }
}

void require_positive(int value, const std::string& name) {
    if (value <= 0) {
        throw std::runtime_error("Invalid config: " + name + " must be positive");
    }
}

void require_non_negative(int value, const std::string& name) {
    if (value < 0) {
        throw std::runtime_error("Invalid config: " + name + " must not be negative");
    }
}

void require_port(int value, const std::string& name) {
    if (value <= 0 || value > 65535) {
        throw std::runtime_error("Invalid config: " + name + " must be between 1 and 65535");
    }
}

void require_path(const std::string& value, const std::string& name) {
    require_non_empty(value, name);
    if (value.front() != '/') {
        throw std::runtime_error("Invalid config: " + name + " must start with /");
    }
}

void validate_config(const Config& config) {
    const std::set<std::string> roles = {
        "standalone", "gateway", "processor", "query", "geofence", "matcher",
    };
    if (!roles.contains(config.server.role)) {
        throw std::runtime_error("Invalid config: server.role is not supported: " + config.server.role);
    }
    require_non_empty(config.server.listen_addr, "server.listen_addr");
    require_port(config.server.grpc_port, "server.grpc_port");
    require_port(config.server.udp_port, "server.udp_port");

    require_non_empty(config.kafka.brokers, "kafka.brokers");
    require_non_empty(config.kafka.ingest_topic, "kafka.ingest_topic");
    require_non_empty(config.kafka.geofence_topic, "kafka.geofence_topic");
    require_non_empty(config.kafka.dlq_topic, "kafka.dlq_topic");
    require_non_empty(config.kafka.consumer_group, "kafka.consumer_group");
    require_positive(config.kafka.num_partitions, "kafka.num_partitions");
    require_positive(config.kafka.batch_size_bytes, "kafka.batch_size_bytes");
    require_non_negative(config.kafka.linger_ms, "kafka.linger_ms");

    require_non_empty(config.redis.addrs, "redis.addrs");
    require_positive(config.redis.pool_size, "redis.pool_size");
    require_positive(config.redis.connect_timeout_ms, "redis.connect_timeout_ms");
    require_positive(config.redis.read_timeout_ms, "redis.read_timeout_ms");
    require_non_empty(config.redis.key_prefix, "redis.key_prefix");
    require_positive(config.redis.device_ttl_s, "redis.device_ttl_s");

    require_non_empty(config.postgis.dsn, "postgis.dsn");
    require_positive(config.postgis.pool_size, "postgis.pool_size");
    require_positive(config.postgis.write_batch_size, "postgis.write_batch_size");
    require_positive(config.postgis.write_flush_interval_ms, "postgis.write_flush_interval_ms");
    require_positive(config.postgis.query_timeout_ms, "postgis.query_timeout_ms");

    require_positive(config.processor.dedup_ttl_s, "processor.dedup_ttl_s");
    require_positive(config.processor.dedup_max_entries, "processor.dedup_max_entries");
    require_non_negative(config.processor.out_of_order_tolerance_s, "processor.out_of_order_tolerance_s");
    require_positive(config.processor.history_batch_size, "processor.history_batch_size");
    require_positive(config.processor.history_flush_interval_ms, "processor.history_flush_interval_ms");
    require_positive(config.processor.history_buffer_max_rows, "processor.history_buffer_max_rows");
    require_non_negative(config.processor.redis_max_retries, "processor.redis_max_retries");
    require_non_negative(config.processor.redis_backoff_max_ms, "processor.redis_backoff_max_ms");

    if (config.spatial.h3_resolution < 0 || config.spatial.h3_resolution > 15) {
        throw std::runtime_error("Invalid config: spatial.h3_resolution must be between 0 and 15");
    }
    require_positive(config.spatial.nearby_max_results, "spatial.nearby_max_results");
    if (config.spatial.nearby_max_radius_m <= 0.0) {
        throw std::runtime_error("Invalid config: spatial.nearby_max_radius_m must be positive");
    }
    require_positive(config.spatial.h3_cache_size_mb, "spatial.h3_cache_size_mb");

    require_positive(config.geofence.dwell_threshold_s, "geofence.dwell_threshold_s");
    require_positive(config.geofence.max_fences, "geofence.max_fences");
    require_positive(config.geofence.reload_interval_s, "geofence.reload_interval_s");

    require_positive(config.gateway.max_batch_events, "gateway.max_batch_events");
    require_positive(config.gateway.rate_limit_rps_per_device, "gateway.rate_limit_rps_per_device");
    require_non_negative(config.gateway.timestamp_skew_tolerance_s, "gateway.timestamp_skew_tolerance_s");
    require_positive(config.gateway.queue_full_timeout_ms, "gateway.queue_full_timeout_ms");
    if (config.gateway.auth_required) {
        require_non_empty(config.gateway.api_key, "gateway.api_key");
    }
    require_non_negative(config.gateway.max_in_flight_requests, "gateway.max_in_flight_requests");

    require_non_empty(config.matching.strategy_name, "matching.strategy_name");
    require_non_empty(config.matching.request_topic, "matching.request_topic");
    require_non_empty(config.matching.result_topic, "matching.result_topic");
    require_positive(config.matching.request_ttl_ms, "matching.request_ttl_ms");

    require_non_negative(config.threads.io_threads, "threads.io_threads");
    require_non_negative(config.threads.processor_threads, "threads.processor_threads");
    require_positive(config.threads.geofence_eval_threads, "threads.geofence_eval_threads");
    require_positive(config.threads.blocking_pool_size, "threads.blocking_pool_size");

    require_non_empty(config.observability.metrics_addr, "observability.metrics_addr");
    require_port(config.observability.metrics_port, "observability.metrics_port");
    require_path(config.observability.metrics_path, "observability.metrics_path");
    require_path(config.observability.health_path, "observability.health_path");
    require_path(config.observability.readiness_path, "observability.readiness_path");
    const std::set<std::string> log_levels = {"trace", "debug", "info", "warn", "error"};
    if (!log_levels.contains(config.observability.log_level)) {
        throw std::runtime_error("Invalid config: observability.log_level is not supported: " +
                                 config.observability.log_level);
    }
}

} // namespace

Config Config::load(const std::string& path) {
    const SectionMap sections = parse_file(path);
    Config config;

    config.server.role = get_string(sections, "server", "role", config.server.role, path);
    config.server.listen_addr = get_string(sections, "server", "listen_addr", config.server.listen_addr, path);
    config.server.grpc_port = get_int(sections, "server", "grpc_port", config.server.grpc_port, path);
    config.server.udp_port = get_int(sections, "server", "udp_port", config.server.udp_port, path);
    config.server.tls_cert = get_string(sections, "server", "tls_cert", config.server.tls_cert, path);
    config.server.tls_key = get_string(sections, "server", "tls_key", config.server.tls_key, path);

    config.kafka.brokers = get_string(sections, "kafka", "brokers", config.kafka.brokers, path);
    config.kafka.ingest_topic = get_string(sections, "kafka", "ingest_topic", config.kafka.ingest_topic, path);
    config.kafka.geofence_topic = get_string(sections, "kafka", "geofence_topic", config.kafka.geofence_topic, path);
    config.kafka.dlq_topic = get_string(sections, "kafka", "dlq_topic", config.kafka.dlq_topic, path);
    config.kafka.consumer_group = get_string(sections, "kafka", "consumer_group", config.kafka.consumer_group, path);
    config.kafka.num_partitions = get_int(sections, "kafka", "num_partitions", config.kafka.num_partitions, path);
    config.kafka.batch_size_bytes = get_int(sections, "kafka", "batch_size_bytes", config.kafka.batch_size_bytes, path);
    config.kafka.linger_ms = get_int(sections, "kafka", "linger_ms", config.kafka.linger_ms, path);

    config.redis.addrs = get_string(sections, "redis", "addrs", config.redis.addrs, path);
    config.redis.pool_size = get_int(sections, "redis", "pool_size", config.redis.pool_size, path);
    config.redis.connect_timeout_ms =
        get_int(sections, "redis", "connect_timeout_ms", config.redis.connect_timeout_ms, path);
    config.redis.read_timeout_ms = get_int(sections, "redis", "read_timeout_ms", config.redis.read_timeout_ms, path);
    config.redis.key_prefix = get_string(sections, "redis", "key_prefix", config.redis.key_prefix, path);
    config.redis.device_ttl_s = get_int(sections, "redis", "device_ttl_s", config.redis.device_ttl_s, path);

    config.postgis.dsn = get_string(sections, "postgis", "dsn", config.postgis.dsn, path);
    config.postgis.pool_size = get_int(sections, "postgis", "pool_size", config.postgis.pool_size, path);
    config.postgis.write_batch_size =
        get_int(sections, "postgis", "write_batch_size", config.postgis.write_batch_size, path);
    config.postgis.write_flush_interval_ms =
        get_int(sections, "postgis", "write_flush_interval_ms", config.postgis.write_flush_interval_ms, path);
    config.postgis.query_timeout_ms =
        get_int(sections, "postgis", "query_timeout_ms", config.postgis.query_timeout_ms, path);

    config.processor.dedup_ttl_s =
        get_int(sections, "processor", "dedup_ttl_s", config.processor.dedup_ttl_s, path);
    config.processor.dedup_max_entries =
        get_int(sections, "processor", "dedup_max_entries", config.processor.dedup_max_entries, path);
    config.processor.sequence_guard_enabled =
        get_bool(sections, "processor", "sequence_guard_enabled", config.processor.sequence_guard_enabled, path);
    config.processor.out_of_order_tolerance_s =
        get_int(sections, "processor", "out_of_order_tolerance_s", config.processor.out_of_order_tolerance_s, path);
    config.processor.history_batch_size =
        get_int(sections, "processor", "history_batch_size", config.processor.history_batch_size, path);
    config.processor.history_flush_interval_ms =
        get_int(sections, "processor", "history_flush_interval_ms", config.processor.history_flush_interval_ms, path);
    config.processor.history_buffer_max_rows =
        get_int(sections, "processor", "history_buffer_max_rows", config.processor.history_buffer_max_rows, path);
    config.processor.redis_max_retries =
        get_int(sections, "processor", "redis_max_retries", config.processor.redis_max_retries, path);
    config.processor.redis_backoff_max_ms =
        get_int(sections, "processor", "redis_backoff_max_ms", config.processor.redis_backoff_max_ms, path);

    config.spatial.h3_resolution =
        get_int(sections, "spatial", "h3_resolution", config.spatial.h3_resolution, path);
    config.spatial.nearby_max_results =
        get_int(sections, "spatial", "nearby_max_results", config.spatial.nearby_max_results, path);
    config.spatial.nearby_max_radius_m =
        get_double(sections, "spatial", "nearby_max_radius_m", config.spatial.nearby_max_radius_m, path);
    config.spatial.h3_cache_size_mb =
        get_int(sections, "spatial", "h3_cache_size_mb", config.spatial.h3_cache_size_mb, path);

    config.geofence.eval_enabled =
        get_bool(sections, "geofence", "eval_enabled", config.geofence.eval_enabled, path);
    config.geofence.dwell_threshold_s =
        get_int(sections, "geofence", "dwell_threshold_s", config.geofence.dwell_threshold_s, path);
    config.geofence.max_fences = get_int(sections, "geofence", "max_fences", config.geofence.max_fences, path);
    config.geofence.reload_interval_s =
        get_int(sections, "geofence", "reload_interval_s", config.geofence.reload_interval_s, path);

    config.gateway.max_batch_events =
        get_int(sections, "gateway", "max_batch_events", config.gateway.max_batch_events, path);
    config.gateway.rate_limit_rps_per_device =
        get_int(sections, "gateway", "rate_limit_rps_per_device", config.gateway.rate_limit_rps_per_device, path);
    config.gateway.timestamp_skew_tolerance_s =
        get_int(sections, "gateway", "timestamp_skew_tolerance_s", config.gateway.timestamp_skew_tolerance_s, path);
    config.gateway.queue_full_timeout_ms =
        get_int(sections, "gateway", "queue_full_timeout_ms", config.gateway.queue_full_timeout_ms, path);
    config.gateway.auth_required =
        get_bool(sections, "gateway", "auth_required", config.gateway.auth_required, path);
    config.gateway.api_key = get_string(sections, "gateway", "api_key", config.gateway.api_key, path);
    config.gateway.max_in_flight_requests =
        get_int(sections, "gateway", "max_in_flight_requests", config.gateway.max_in_flight_requests, path);

    config.matching.strategy_name =
        get_string(sections, "matching", "strategy_name", config.matching.strategy_name, path);
    config.matching.request_topic =
        get_string(sections, "matching", "request_topic", config.matching.request_topic, path);
    config.matching.result_topic =
        get_string(sections, "matching", "result_topic", config.matching.result_topic, path);
    config.matching.request_ttl_ms =
        get_int(sections, "matching", "request_ttl_ms", config.matching.request_ttl_ms, path);

    config.threads.io_threads = get_int(sections, "threads", "io_threads", config.threads.io_threads, path);
    config.threads.processor_threads =
        get_int(sections, "threads", "processor_threads", config.threads.processor_threads, path);
    config.threads.geofence_eval_threads =
        get_int(sections, "threads", "geofence_eval_threads", config.threads.geofence_eval_threads, path);
    config.threads.blocking_pool_size =
        get_int(sections, "threads", "blocking_pool_size", config.threads.blocking_pool_size, path);

    config.observability.metrics_addr =
        get_string(sections, "observability", "metrics_addr", config.observability.metrics_addr, path);
    config.observability.metrics_port =
        get_int(sections, "observability", "metrics_port", config.observability.metrics_port, path);
    config.observability.metrics_path =
        get_string(sections, "observability", "metrics_path", config.observability.metrics_path, path);
    config.observability.log_level =
        get_string(sections, "observability", "log_level", config.observability.log_level, path);
    config.observability.admin_http_enabled =
        get_bool(sections, "observability", "admin_http_enabled", config.observability.admin_http_enabled, path);
    config.observability.health_path =
        get_string(sections, "observability", "health_path", config.observability.health_path, path);
    config.observability.readiness_path =
        get_string(sections, "observability", "readiness_path", config.observability.readiness_path, path);
    config.observability.require_kafka_readiness =
        get_bool(sections,
                 "observability",
                 "require_kafka_readiness",
                 config.observability.require_kafka_readiness,
                 path);
    config.observability.require_redis_readiness =
        get_bool(sections,
                 "observability",
                 "require_redis_readiness",
                 config.observability.require_redis_readiness,
                 path);
    config.observability.require_postgis_readiness =
        get_bool(sections,
                 "observability",
                 "require_postgis_readiness",
                 config.observability.require_postgis_readiness,
                 path);
    config.observability.require_h3_readiness =
        get_bool(sections,
                 "observability",
                 "require_h3_readiness",
                 config.observability.require_h3_readiness,
                 path);

    config.validate();
    return config;
}

void Config::validate() const {
    validate_config(*this);
}

} // namespace signalroute
