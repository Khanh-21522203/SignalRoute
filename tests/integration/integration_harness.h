#pragma once

#include <array>
#include <string_view>

namespace signalroute::integration {

struct FeatureGroup {
    std::string_view id;
    std::string_view ctest_label;
    std::string_view description;
    std::string_view required_services;
    std::string_view production_switches;
};

inline constexpr std::array<FeatureGroup, 6> kFeatureGroups{{
    {"ingestion_pipeline", "integration:ingestion", "Gateway publish through processor consume", "redpanda", "SR_ENABLE_REAL_KAFKA;SR_ENABLE_PROTOBUF"},
    {"state_persistence", "integration:state", "Processor state writes and latest-state reads", "redis", "SR_ENABLE_REAL_REDIS"},
    {"trip_history", "integration:trip-history", "History writes and trip replay queries", "postgis", "SR_ENABLE_REAL_POSTGIS"},
    {"nearby_query", "integration:nearby", "Nearby query over real spatial/state indexes", "redis;postgis;h3", "SR_ENABLE_REAL_REDIS;SR_ENABLE_REAL_POSTGIS;SR_ENABLE_REAL_H3"},
    {"geofence_events", "integration:geofence", "Fence load, evaluation, audit, and event publication", "redpanda;postgis;h3", "SR_ENABLE_REAL_KAFKA;SR_ENABLE_REAL_POSTGIS;SR_ENABLE_REAL_H3;SR_ENABLE_PROTOBUF"},
    {"matching_reservation", "integration:matching", "Matching request/result loop and reservation persistence", "redpanda;redis", "SR_ENABLE_REAL_KAFKA;SR_ENABLE_REAL_REDIS;SR_ENABLE_PROTOBUF"},
}};

inline bool is_feature_group(std::string_view id) {
    for (const auto& group : kFeatureGroups) {
        if (group.id == id) {
            return true;
        }
    }
    return false;
}

} // namespace signalroute::integration
