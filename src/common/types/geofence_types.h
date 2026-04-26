#pragma once

/**
 * SignalRoute — Geofence domain types
 *
 * Shared types for geofence rules, fence state, and events.
 */

#include <string>
#include <vector>
#include <unordered_set>
#include <utility>
#include <cstdint>

namespace signalroute {

/// Per-device fence state.
enum class FenceState {
    OUTSIDE,
    INSIDE,
    DWELL
};

/// Geofence event types.
enum class GeofenceEventType {
    ENTER,
    EXIT,
    DWELL
};

/**
 * A registered geofence rule.
 * Loaded from PostGIS at startup and cached in FenceRegistry.
 */
struct GeofenceRule {
    std::string                fence_id;
    std::string                name;
    std::vector<std::pair<double, double>> polygon_vertices; /// (lat, lon) pairs
    std::unordered_set<int64_t>           h3_cells;          /// H3 polyfill cell set
    int                        dwell_threshold_s = 300;
    bool                       active            = true;
    bool                       complex_geometry  = false;     /// True if concave or has holes

    // TODO: Add methods for serialization from PostGIS rows
};

/**
 * A geofence event emitted when a device transitions fence state.
 */
struct GeofenceEventRecord {
    std::string        device_id;
    std::string        fence_id;
    std::string        fence_name;
    GeofenceEventType  event_type;
    double             lat          = 0.0;
    double             lon          = 0.0;
    int64_t            event_ts_ms  = 0;
    int                inside_duration_s = 0;  /// For DWELL events

    // TODO: Add conversion to Protobuf GeofenceEvent
};

/// Helper to convert FenceState to string for Redis storage.
inline const char* fence_state_to_string(FenceState state) {
    switch (state) {
        case FenceState::OUTSIDE: return "OUTSIDE";
        case FenceState::INSIDE:  return "INSIDE";
        case FenceState::DWELL:   return "DWELL";
    }
    return "UNKNOWN";
}

/// Helper to parse FenceState from string.
inline FenceState fence_state_from_string(const std::string& s) {
    if (s == "INSIDE") return FenceState::INSIDE;
    if (s == "DWELL")  return FenceState::DWELL;
    return FenceState::OUTSIDE; // default
}

/// Helper to convert GeofenceEventType to string for event payloads/storage.
inline const char* geofence_event_type_to_string(GeofenceEventType type) {
    switch (type) {
        case GeofenceEventType::ENTER: return "ENTER";
        case GeofenceEventType::EXIT:  return "EXIT";
        case GeofenceEventType::DWELL: return "DWELL";
    }
    return "UNKNOWN";
}

/// Helper to parse GeofenceEventType from string.
inline GeofenceEventType geofence_event_type_from_string(const std::string& s) {
    if (s == "EXIT")  return GeofenceEventType::EXIT;
    if (s == "DWELL") return GeofenceEventType::DWELL;
    return GeofenceEventType::ENTER; // default
}

} // namespace signalroute
