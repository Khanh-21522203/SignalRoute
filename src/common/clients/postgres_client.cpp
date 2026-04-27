#include "postgres_client.h"
#include "../spatial/haversine.h"

#if SIGNALROUTE_HAS_POSTGIS
#include <libpq-fe.h>
#endif

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace signalroute {
namespace {

int64_t event_time_ms(const LocationEvent& event) {
    return event.timestamp_ms != 0 ? event.timestamp_ms : event.server_recv_ms;
}

bool in_time_range(const LocationEvent& event, int64_t from_ts, int64_t to_ts) {
    const int64_t ts = event_time_ms(event);
    return ts >= from_ts && ts <= to_ts;
}

void sort_trip_points(std::vector<LocationEvent>& events) {
    std::sort(events.begin(), events.end(), [](const LocationEvent& lhs, const LocationEvent& rhs) {
        const int64_t lhs_time = event_time_ms(lhs);
        const int64_t rhs_time = event_time_ms(rhs);
        if (lhs_time != rhs_time) {
            return lhs_time < rhs_time;
        }
        if (lhs.device_id != rhs.device_id) {
            return lhs.device_id < rhs.device_id;
        }
        return lhs.seq < rhs.seq;
    });
}

std::string to_string(double value) {
    return std::to_string(value);
}

std::string to_string(float value) {
    return std::to_string(value);
}

std::string to_string(int value) {
    return std::to_string(value);
}

std::string to_string(int64_t value) {
    return std::to_string(value);
}

std::string to_string(uint64_t value) {
    return std::to_string(value);
}

#if SIGNALROUTE_HAS_POSTGIS

struct PGResultDeleter {
    void operator()(PGresult* result) const {
        if (result != nullptr) {
            PQclear(result);
        }
    }
};

using ResultPtr = std::unique_ptr<PGresult, PGResultDeleter>;

void throw_pg_error(PGconn* conn, const std::string& operation) {
    const char* message = conn == nullptr ? "no PostgreSQL connection" : PQerrorMessage(conn);
    throw std::runtime_error(operation + " failed: " + (message == nullptr ? "unknown error" : message));
}

ResultPtr exec_command(PGconn* conn, const std::string& sql, const std::string& operation) {
    ResultPtr result(PQexec(conn, sql.c_str()));
    if (PQresultStatus(result.get()) != PGRES_COMMAND_OK) {
        throw_pg_error(conn, operation);
    }
    return result;
}

ResultPtr exec_params(
    PGconn* conn,
    const std::string& sql,
    const std::vector<std::string>& params,
    ExecStatusType expected_status,
    const std::string& operation) {
    std::vector<const char*> values;
    values.reserve(params.size());
    for (const auto& param : params) {
        values.push_back(param.c_str());
    }

    ResultPtr result(PQexecParams(
        conn,
        sql.c_str(),
        static_cast<int>(values.size()),
        nullptr,
        values.data(),
        nullptr,
        nullptr,
        0));
    if (PQresultStatus(result.get()) != expected_status) {
        throw_pg_error(conn, operation);
    }
    return result;
}

int64_t parse_i64(const char* value) {
    return value == nullptr || *value == '\0' ? 0 : std::strtoll(value, nullptr, 10);
}

uint64_t parse_u64(const char* value) {
    return value == nullptr || *value == '\0' ? 0 : static_cast<uint64_t>(std::strtoull(value, nullptr, 10));
}

double parse_double(const char* value) {
    return value == nullptr || *value == '\0' ? 0.0 : std::strtod(value, nullptr);
}

float parse_float(const char* value) {
    return static_cast<float>(parse_double(value));
}

bool parse_bool(const char* value) {
    return value != nullptr && (std::strcmp(value, "t") == 0 || std::strcmp(value, "true") == 0 || std::strcmp(value, "1") == 0);
}

std::vector<int64_t> parse_h3_cells(std::string_view csv) {
    std::vector<int64_t> cells;
    while (!csv.empty()) {
        const auto comma = csv.find(',');
        const std::string_view token = csv.substr(0, comma);
        if (!token.empty()) {
            int64_t value = 0;
            const auto* begin = token.data();
            const auto* end = begin + token.size();
            const auto [ptr, ec] = std::from_chars(begin, end, value);
            if (ec == std::errc{} && ptr == end) {
                cells.push_back(value);
            }
        }
        if (comma == std::string_view::npos) {
            break;
        }
        csv.remove_prefix(comma + 1);
    }
    return cells;
}

std::optional<std::pair<double, double>> parse_lon_lat_pair(std::string_view pair) {
    while (!pair.empty() && pair.front() == ' ') {
        pair.remove_prefix(1);
    }
    while (!pair.empty() && pair.back() == ' ') {
        pair.remove_suffix(1);
    }
    const auto space = pair.find(' ');
    if (space == std::string_view::npos) {
        return std::nullopt;
    }
    const std::string lon_text(pair.substr(0, space));
    std::string_view lat_view = pair.substr(space + 1);
    while (!lat_view.empty() && lat_view.front() == ' ') {
        lat_view.remove_prefix(1);
    }
    const std::string lat_text(lat_view);
    return std::pair<double, double>{std::strtod(lat_text.c_str(), nullptr), std::strtod(lon_text.c_str(), nullptr)};
}

std::vector<std::pair<double, double>> parse_polygon_wkt(std::string_view wkt) {
    constexpr std::string_view prefix = "POLYGON((";
    if (!wkt.starts_with(prefix) || wkt.size() <= prefix.size() + 2) {
        return {};
    }
    wkt.remove_prefix(prefix.size());
    wkt.remove_suffix(2);

    std::vector<std::pair<double, double>> vertices;
    while (!wkt.empty()) {
        const auto comma = wkt.find(',');
        const auto token = wkt.substr(0, comma);
        if (const auto parsed = parse_lon_lat_pair(token)) {
            vertices.push_back(*parsed);
        }
        if (comma == std::string_view::npos) {
            break;
        }
        wkt.remove_prefix(comma + 1);
    }

    if (vertices.size() > 1 && vertices.front() == vertices.back()) {
        vertices.pop_back();
    }
    return vertices;
}

std::string polygon_to_wkt(const std::vector<std::pair<double, double>>& vertices) {
    if (vertices.empty()) {
        return "POLYGON EMPTY";
    }

    std::ostringstream out;
    out << "POLYGON((";
    for (size_t i = 0; i < vertices.size(); ++i) {
        if (i != 0) {
            out << ',';
        }
        out << vertices[i].second << ' ' << vertices[i].first;
    }
    if (vertices.front() != vertices.back()) {
        out << ',' << vertices.front().second << ' ' << vertices.front().first;
    }
    out << "))";
    return out.str();
}

std::string h3_cells_to_csv(const std::unordered_set<int64_t>& cells) {
    std::vector<int64_t> sorted(cells.begin(), cells.end());
    std::sort(sorted.begin(), sorted.end());

    std::ostringstream out;
    for (size_t i = 0; i < sorted.size(); ++i) {
        if (i != 0) {
            out << ',';
        }
        out << sorted[i];
    }
    return out.str();
}

LocationEvent location_event_from_row(PGresult* result, int row) {
    LocationEvent event;
    event.device_id = PQgetvalue(result, row, 0);
    event.seq = parse_u64(PQgetvalue(result, row, 1));
    event.timestamp_ms = parse_i64(PQgetvalue(result, row, 2));
    event.server_recv_ms = parse_i64(PQgetvalue(result, row, 3));
    event.lat = parse_double(PQgetvalue(result, row, 4));
    event.lon = parse_double(PQgetvalue(result, row, 5));
    event.altitude_m = parse_float(PQgetvalue(result, row, 6));
    event.accuracy_m = parse_float(PQgetvalue(result, row, 7));
    event.speed_ms = parse_float(PQgetvalue(result, row, 8));
    event.heading_deg = parse_float(PQgetvalue(result, row, 9));
    return event;
}

GeofenceEventRecord geofence_event_from_row(PGresult* result, int row) {
    GeofenceEventRecord event;
    event.device_id = PQgetvalue(result, row, 0);
    event.fence_id = PQgetvalue(result, row, 1);
    event.fence_name = PQgetvalue(result, row, 2);
    event.event_type = geofence_event_type_from_string(PQgetvalue(result, row, 3));
    event.lat = parse_double(PQgetvalue(result, row, 4));
    event.lon = parse_double(PQgetvalue(result, row, 5));
    event.event_ts_ms = parse_i64(PQgetvalue(result, row, 6));
    event.inside_duration_s = static_cast<int>(parse_i64(PQgetvalue(result, row, 7)));
    return event;
}

#endif

} // namespace

struct PostgresClient::Impl {
#if SIGNALROUTE_HAS_POSTGIS
    explicit Impl(const PostGISConfig& config) {
        conn.reset(PQconnectdb(config.dsn.c_str()));
        if (!conn || PQstatus(conn.get()) != CONNECTION_OK) {
            throw_pg_error(conn.get(), "connect to PostgreSQL");
        }
        exec_params(conn.get(), "SET statement_timeout = $1::integer", {
            to_string(config.query_timeout_ms),
        }, PGRES_COMMAND_OK, "set PostgreSQL statement timeout");
    }

    struct ConnectionDeleter {
        void operator()(PGconn* connection) const {
            if (connection != nullptr) {
                PQfinish(connection);
            }
        }
    };

    std::unique_ptr<PGconn, ConnectionDeleter> conn;
#endif
};

PostgresClient::PostgresClient(const PostGISConfig& config) : config_(config) {
#if SIGNALROUTE_HAS_POSTGIS
    impl_ = std::make_unique<Impl>(config_);
#else
    std::cerr << "[PostgresClient] WARNING: using in-memory PostGIS fallback.\n";
#endif
}

PostgresClient::~PostgresClient() = default;

bool PostgresClient::ping() {
#if SIGNALROUTE_HAS_POSTGIS
    auto result = exec_params(impl_->conn.get(), "SELECT 1", {}, PGRES_TUPLES_OK, "PostgreSQL ping");
    return PQntuples(result.get()) == 1 && std::string_view(PQgetvalue(result.get(), 0, 0)) == "1";
#else
    return true;
#endif
}

void PostgresClient::batch_insert_trip_points(const std::vector<LocationEvent>& events) {
#if SIGNALROUTE_HAS_POSTGIS
    if (events.empty()) {
        return;
    }

    exec_command(impl_->conn.get(), "BEGIN", "begin trip point insert");
    try {
        static const std::string sql =
            "INSERT INTO trip_points ("
            "device_id, seq, event_time, recv_time, location, altitude_m, accuracy_m, speed_ms, heading_deg, h3_r7, metadata) "
            "VALUES ($1, $2::bigint, to_timestamp($3::double precision / 1000.0), "
            "to_timestamp($4::double precision / 1000.0), "
            "ST_SetSRID(ST_MakePoint($6::double precision, $5::double precision), 4326)::geography, "
            "$7::real, $8::real, $9::real, $10::real, NULL, '{}'::jsonb) "
            "ON CONFLICT DO NOTHING";
        for (const auto& event : events) {
            exec_params(impl_->conn.get(), sql, {
                event.device_id,
                to_string(event.seq),
                to_string(event_time_ms(event)),
                to_string(event.server_recv_ms),
                to_string(event.lat),
                to_string(event.lon),
                to_string(event.altitude_m),
                to_string(event.accuracy_m),
                to_string(event.speed_ms),
                to_string(event.heading_deg),
            }, PGRES_COMMAND_OK, "insert trip point");
        }
        exec_command(impl_->conn.get(), "COMMIT", "commit trip point insert");
    } catch (...) {
        try {
            exec_command(impl_->conn.get(), "ROLLBACK", "rollback trip point insert");
        } catch (...) {
        }
        throw;
    }
#else
    std::lock_guard lock(mu_);
    for (const auto& event : events) {
        auto [_, inserted] = trip_point_keys_.emplace(event.device_id, event.seq);
        if (inserted) {
            trip_points_.push_back(event);
        }
    }
#endif
}

std::vector<LocationEvent> PostgresClient::query_trip(
    const std::string& device_id,
    int64_t from_ts, int64_t to_ts,
    int limit)
{
    if (limit <= 0 || from_ts > to_ts) {
        return {};
    }

#if SIGNALROUTE_HAS_POSTGIS
    static const std::string sql =
        "SELECT device_id, seq, "
        "(EXTRACT(EPOCH FROM event_time) * 1000)::bigint AS event_time_ms, "
        "(EXTRACT(EPOCH FROM recv_time) * 1000)::bigint AS recv_time_ms, "
        "ST_Y(location::geometry) AS lat, ST_X(location::geometry) AS lon, "
        "COALESCE(altitude_m, 0), COALESCE(accuracy_m, 0), COALESCE(speed_ms, 0), COALESCE(heading_deg, 0) "
        "FROM trip_points "
        "WHERE device_id = $1 "
        "AND event_time BETWEEN to_timestamp($2::double precision / 1000.0) "
        "AND to_timestamp($3::double precision / 1000.0) "
        "ORDER BY event_time ASC, seq ASC "
        "LIMIT $4::integer";
    auto result = exec_params(impl_->conn.get(), sql, {
        device_id,
        to_string(from_ts),
        to_string(to_ts),
        to_string(limit),
    }, PGRES_TUPLES_OK, "query trip points");

    std::vector<LocationEvent> events;
    events.reserve(static_cast<size_t>(PQntuples(result.get())));
    for (int row = 0; row < PQntuples(result.get()); ++row) {
        events.push_back(location_event_from_row(result.get(), row));
    }
    return events;
#else
    std::lock_guard lock(mu_);
    std::vector<LocationEvent> result;
    for (const auto& event : trip_points_) {
        if (event.device_id == device_id && in_time_range(event, from_ts, to_ts)) {
            result.push_back(event);
        }
    }

    sort_trip_points(result);
    if (static_cast<int>(result.size()) > limit) {
        result.resize(static_cast<size_t>(limit));
    }
    return result;
#endif
}

std::vector<LocationEvent> PostgresClient::query_trip_spatial(
    const std::string& device_id,
    int64_t from_ts, int64_t to_ts,
    double center_lat, double center_lon, double radius_m,
    int limit)
{
    if (limit <= 0 || radius_m < 0.0 || from_ts > to_ts) {
        return {};
    }

#if SIGNALROUTE_HAS_POSTGIS
    static const std::string sql =
        "SELECT device_id, seq, "
        "(EXTRACT(EPOCH FROM event_time) * 1000)::bigint AS event_time_ms, "
        "(EXTRACT(EPOCH FROM recv_time) * 1000)::bigint AS recv_time_ms, "
        "ST_Y(location::geometry) AS lat, ST_X(location::geometry) AS lon, "
        "COALESCE(altitude_m, 0), COALESCE(accuracy_m, 0), COALESCE(speed_ms, 0), COALESCE(heading_deg, 0) "
        "FROM trip_points "
        "WHERE device_id = $1 "
        "AND event_time BETWEEN to_timestamp($2::double precision / 1000.0) "
        "AND to_timestamp($3::double precision / 1000.0) "
        "AND ST_DWithin(location, ST_SetSRID(ST_MakePoint($5::double precision, $4::double precision), 4326)::geography, $6::double precision) "
        "ORDER BY event_time ASC, seq ASC "
        "LIMIT $7::integer";
    auto result = exec_params(impl_->conn.get(), sql, {
        device_id,
        to_string(from_ts),
        to_string(to_ts),
        to_string(center_lat),
        to_string(center_lon),
        to_string(radius_m),
        to_string(limit),
    }, PGRES_TUPLES_OK, "query spatial trip points");

    std::vector<LocationEvent> events;
    events.reserve(static_cast<size_t>(PQntuples(result.get())));
    for (int row = 0; row < PQntuples(result.get()); ++row) {
        events.push_back(location_event_from_row(result.get(), row));
    }
    return events;
#else
    std::lock_guard lock(mu_);
    std::vector<LocationEvent> result;
    for (const auto& event : trip_points_) {
        if (event.device_id != device_id || !in_time_range(event, from_ts, to_ts)) {
            continue;
        }
        const double distance_m = geo::haversine(center_lat, center_lon, event.lat, event.lon);
        if (distance_m <= radius_m) {
            result.push_back(event);
        }
    }

    sort_trip_points(result);
    if (static_cast<int>(result.size()) > limit) {
        result.resize(static_cast<size_t>(limit));
    }
    return result;
#endif
}

std::vector<GeofenceRule> PostgresClient::load_active_fences() {
#if SIGNALROUTE_HAS_POSTGIS
    static const std::string sql =
        "SELECT fence_id, name, dwell_threshold_s, active, "
        "COALESCE(array_to_string(h3_cells, ','), ''), COALESCE(ST_AsText(geometry), '') "
        "FROM geofence_rules "
        "WHERE active = true "
        "ORDER BY fence_id ASC";
    auto result = exec_params(impl_->conn.get(), sql, {}, PGRES_TUPLES_OK, "load active fences");

    std::vector<GeofenceRule> fences;
    fences.reserve(static_cast<size_t>(PQntuples(result.get())));
    for (int row = 0; row < PQntuples(result.get()); ++row) {
        GeofenceRule fence;
        fence.fence_id = PQgetvalue(result.get(), row, 0);
        fence.name = PQgetvalue(result.get(), row, 1);
        fence.dwell_threshold_s = static_cast<int>(parse_i64(PQgetvalue(result.get(), row, 2)));
        fence.active = parse_bool(PQgetvalue(result.get(), row, 3));
        for (const int64_t cell : parse_h3_cells(PQgetvalue(result.get(), row, 4))) {
            fence.h3_cells.insert(cell);
        }
        fence.polygon_vertices = parse_polygon_wkt(PQgetvalue(result.get(), row, 5));
        fence.complex_geometry = fence.polygon_vertices.size() < 3;
        fences.push_back(std::move(fence));
    }
    return fences;
#else
    std::lock_guard lock(mu_);
    std::vector<GeofenceRule> result;
    for (const auto& fence : active_fences_) {
        if (fence.active) {
            result.push_back(fence);
        }
    }
    return result;
#endif
}

void PostgresClient::insert_geofence_event(const GeofenceEventRecord& event) {
#if SIGNALROUTE_HAS_POSTGIS
    static const std::string sql =
        "INSERT INTO geofence_events ("
        "device_id, fence_id, fence_name, event_type, lat, lon, event_ts, inside_duration_s) "
        "VALUES ($1, $2, $3, $4, $5::double precision, $6::double precision, "
        "to_timestamp($7::double precision / 1000.0), $8::integer)";
    exec_params(impl_->conn.get(), sql, {
        event.device_id,
        event.fence_id,
        event.fence_name,
        geofence_event_type_to_string(event.event_type),
        to_string(event.lat),
        to_string(event.lon),
        to_string(event.event_ts_ms),
        to_string(event.inside_duration_s),
    }, PGRES_COMMAND_OK, "insert geofence event");
#else
    std::lock_guard lock(mu_);
    geofence_events_.push_back(event);
#endif
}

size_t PostgresClient::trip_point_count() const {
#if SIGNALROUTE_HAS_POSTGIS
    auto result = exec_params(impl_->conn.get(), "SELECT COUNT(*) FROM trip_points", {}, PGRES_TUPLES_OK, "count trip points");
    return static_cast<size_t>(parse_u64(PQgetvalue(result.get(), 0, 0)));
#else
    std::lock_guard lock(mu_);
    return trip_points_.size();
#endif
}

size_t PostgresClient::geofence_event_count() const {
#if SIGNALROUTE_HAS_POSTGIS
    auto result = exec_params(impl_->conn.get(), "SELECT COUNT(*) FROM geofence_events", {}, PGRES_TUPLES_OK, "count geofence events");
    return static_cast<size_t>(parse_u64(PQgetvalue(result.get(), 0, 0)));
#else
    std::lock_guard lock(mu_);
    return geofence_events_.size();
#endif
}

std::vector<GeofenceEventRecord> PostgresClient::geofence_events() const {
#if SIGNALROUTE_HAS_POSTGIS
    static const std::string sql =
        "SELECT device_id, fence_id, COALESCE(fence_name, ''), event_type, lat, lon, "
        "(EXTRACT(EPOCH FROM event_ts) * 1000)::bigint, COALESCE(inside_duration_s, 0) "
        "FROM geofence_events ORDER BY event_ts ASC, id ASC";
    auto result = exec_params(impl_->conn.get(), sql, {}, PGRES_TUPLES_OK, "load geofence events");

    std::vector<GeofenceEventRecord> events;
    events.reserve(static_cast<size_t>(PQntuples(result.get())));
    for (int row = 0; row < PQntuples(result.get()); ++row) {
        events.push_back(geofence_event_from_row(result.get(), row));
    }
    return events;
#else
    std::lock_guard lock(mu_);
    return geofence_events_;
#endif
}

void PostgresClient::set_active_fences(std::vector<GeofenceRule> fences) {
#if SIGNALROUTE_HAS_POSTGIS
    exec_command(impl_->conn.get(), "BEGIN", "begin fence upsert");
    try {
        static const std::string sql =
            "INSERT INTO geofence_rules (fence_id, name, geometry, h3_cells, dwell_threshold_s, active) "
            "VALUES ($1, $2, ST_GeomFromText($3, 4326), "
            "CASE WHEN $4 = '' THEN ARRAY[]::bigint[] ELSE string_to_array($4, ',')::bigint[] END, "
            "$5::integer, $6::boolean) "
            "ON CONFLICT (fence_id) DO UPDATE SET "
            "name = EXCLUDED.name, geometry = EXCLUDED.geometry, h3_cells = EXCLUDED.h3_cells, "
            "dwell_threshold_s = EXCLUDED.dwell_threshold_s, active = EXCLUDED.active, updated_at = NOW()";
        for (const auto& fence : fences) {
            exec_params(impl_->conn.get(), sql, {
                fence.fence_id,
                fence.name,
                polygon_to_wkt(fence.polygon_vertices),
                h3_cells_to_csv(fence.h3_cells),
                to_string(fence.dwell_threshold_s),
                fence.active ? "true" : "false",
            }, PGRES_COMMAND_OK, "upsert geofence rule");
        }
        exec_command(impl_->conn.get(), "COMMIT", "commit fence upsert");
    } catch (...) {
        try {
            exec_command(impl_->conn.get(), "ROLLBACK", "rollback fence upsert");
        } catch (...) {
        }
        throw;
    }
#else
    std::lock_guard lock(mu_);
    active_fences_ = std::move(fences);
#endif
}

} // namespace signalroute
