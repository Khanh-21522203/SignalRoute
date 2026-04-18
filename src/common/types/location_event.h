#pragma once

/**
 * SignalRoute — LocationEvent domain type
 *
 * Internal representation of a GPS location event.
 * Decoupled from Protobuf wire format — conversion functions
 * live at the service boundary (gateway, processor).
 */

#include <string>
#include <unordered_map>
#include <cstdint>

namespace signalroute {

struct LocationEvent {
    std::string device_id;         /// Unique device identifier
    double      lat         = 0.0; /// WGS-84 latitude [-90, 90]
    double      lon         = 0.0; /// WGS-84 longitude [-180, 180]
    float       altitude_m  = 0.0f; /// Altitude in meters
    float       accuracy_m  = 0.0f; /// GPS accuracy radius in meters
    float       speed_ms    = 0.0f; /// Speed in meters/second
    float       heading_deg = 0.0f; /// Heading [0, 360), 0 = North
    int64_t     timestamp_ms = 0;   /// Device clock, Unix epoch ms
    int64_t     server_recv_ms = 0; /// Gateway receive time
    uint64_t    seq          = 0;   /// Monotonically increasing per device
    std::unordered_map<std::string, std::string> metadata;

    // TODO: Add conversion methods to/from Protobuf
    // static LocationEvent from_proto(const proto::LocationEvent& pb);
    // proto::LocationEvent to_proto() const;
};

} // namespace signalroute
