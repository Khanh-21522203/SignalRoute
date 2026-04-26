#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace signalroute::wire {

struct LocationEventMessage {
    std::string device_id;
    double lat = 0.0;
    double lon = 0.0;
    float altitude_m = 0.0f;
    float accuracy_m = 0.0f;
    float speed_ms = 0.0f;
    float heading_deg = 0.0f;
    int64_t timestamp_ms = 0;
    int64_t server_recv_ms = 0;
    uint64_t seq = 0;
    std::unordered_map<std::string, std::string> metadata;
};

struct DeviceStateResponseMessage {
    std::string device_id;
    double lat = 0.0;
    double lon = 0.0;
    float altitude_m = 0.0f;
    float accuracy_m = 0.0f;
    float speed_ms = 0.0f;
    float heading_deg = 0.0f;
    int64_t h3_cell = 0;
    uint64_t seq = 0;
    int64_t updated_at = 0;
};

enum class GeofenceEventTypeMessage {
    GEOFENCE_ENTER = 0,
    GEOFENCE_EXIT = 1,
    GEOFENCE_DWELL = 2,
};

struct GeofenceEventMessage {
    std::string device_id;
    std::string fence_id;
    std::string fence_name;
    GeofenceEventTypeMessage event_type = GeofenceEventTypeMessage::GEOFENCE_ENTER;
    double lat = 0.0;
    double lon = 0.0;
    int64_t event_ts_ms = 0;
    int32_t inside_duration_s = 0;
};

struct MatchRequestMessage {
    std::string request_id;
    std::string requester_id;
    double lat = 0.0;
    double lon = 0.0;
    double radius_m = 0.0;
    int32_t max_agents = 0;
    int64_t deadline_ms = 0;
    std::string strategy;
    std::unordered_map<std::string, std::string> metadata;
};

enum class MatchStatusMessage {
    MATCH_SUCCESS = 0,
    MATCH_NO_CANDIDATES = 1,
    MATCH_FAILED = 2,
    MATCH_EXPIRED = 3,
};

struct MatchResultMessage {
    std::string request_id;
    std::string requester_id;
    MatchStatusMessage status = MatchStatusMessage::MATCH_FAILED;
    std::vector<std::string> assigned_agent_ids;
    int64_t latency_ms = 0;
    std::string strategy;
    std::string failure_reason;
};

} // namespace signalroute::wire
