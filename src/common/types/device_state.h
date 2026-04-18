#pragma once

/**
 * SignalRoute — DeviceState domain type
 *
 * Represents the latest known state of a device as persisted in Redis.
 * One record per device.
 */

#include <string>
#include <cstdint>

namespace signalroute {

struct DeviceState {
    std::string device_id;
    double      lat         = 0.0;
    double      lon         = 0.0;
    float       altitude_m  = 0.0f;
    float       accuracy_m  = 0.0f;
    float       speed_ms    = 0.0f;
    float       heading_deg = 0.0f;
    int64_t     h3_cell     = 0;     /// H3 cell ID at configured resolution
    uint64_t    seq         = 0;     /// Last accepted sequence number
    int64_t     updated_at  = 0;     /// Unix epoch ms (server time of write)

    // TODO: Add serialization to/from Redis hash fields
    // std::unordered_map<std::string, std::string> to_redis_hash() const;
    // static DeviceState from_redis_hash(const std::string& device_id,
    //                                     const std::unordered_map<std::string, std::string>& fields);
};

} // namespace signalroute
