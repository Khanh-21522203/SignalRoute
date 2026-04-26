#include "geofence_payload_codec.h"

#if SIGNALROUTE_HAS_PROTOBUF
#include "generated_conversions.h"
#endif

#include <exception>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace signalroute::proto_boundary {
namespace {

constexpr std::string_view kProtobufGeofenceEventPrefix = "SRGF1:";

std::vector<std::string> split_csv(const std::string& payload) {
    std::vector<std::string> fields;
    std::stringstream in(payload);
    std::string field;
    while (std::getline(in, field, ',')) {
        fields.push_back(field);
    }
    return fields;
}

std::string encode_csv_geofence_event_payload(const GeofenceEventRecord& event) {
    std::ostringstream out;
    out << event.device_id << ','
        << event.fence_id << ','
        << geofence_event_type_to_string(event.event_type) << ','
        << event.event_ts_ms;

    if (event.event_type == GeofenceEventType::DWELL) {
        out << ',' << event.inside_duration_s;
    } else {
        out << ',' << event.lat << ',' << event.lon;
    }

    return out.str();
}

Result<GeofenceEventRecord, std::string> decode_csv_geofence_event_payload(const std::string& payload) {
    const auto fields = split_csv(payload);
    if (fields.size() != 5 && fields.size() != 6) {
        return Result<GeofenceEventRecord, std::string>::err("invalid geofence event payload");
    }

    try {
        GeofenceEventRecord event;
        event.device_id = fields[0];
        event.fence_id = fields[1];
        event.event_type = geofence_event_type_from_string(fields[2]);
        event.event_ts_ms = std::stoll(fields[3]);

        if (fields.size() == 5) {
            event.inside_duration_s = std::stoi(fields[4]);
        } else {
            event.lat = std::stod(fields[4]);
            event.lon = std::stod(fields[5]);
        }

        if (event.device_id.empty() || event.fence_id.empty()) {
            return Result<GeofenceEventRecord, std::string>::err("geofence event identifiers are required");
        }
        return Result<GeofenceEventRecord, std::string>::ok(std::move(event));
    } catch (const std::exception&) {
        return Result<GeofenceEventRecord, std::string>::err("invalid geofence event payload");
    }
}

bool has_prefix(const std::string& payload, std::string_view prefix) {
    return payload.size() >= prefix.size() &&
           payload.compare(0, prefix.size(), prefix.data(), prefix.size()) == 0;
}

} // namespace

std::string_view protobuf_geofence_event_payload_prefix() {
    return kProtobufGeofenceEventPrefix;
}

std::string encode_geofence_event_payload(const GeofenceEventRecord& event) {
#if SIGNALROUTE_HAS_PROTOBUF
    auto message = to_generated_geofence_event(event);
    std::string bytes;
    if (message.SerializeToString(&bytes)) {
        return std::string(kProtobufGeofenceEventPrefix) + bytes;
    }
#endif
    return encode_csv_geofence_event_payload(event);
}

Result<GeofenceEventRecord, std::string> decode_geofence_event_payload(const std::string& payload) {
#if SIGNALROUTE_HAS_PROTOBUF
    if (has_prefix(payload, kProtobufGeofenceEventPrefix)) {
        signalroute::v1::GeofenceEvent message;
        const std::string bytes = payload.substr(kProtobufGeofenceEventPrefix.size());
        if (!message.ParseFromString(bytes)) {
            return Result<GeofenceEventRecord, std::string>::err("invalid protobuf geofence event payload");
        }
        return geofence_event_from_generated(message);
    }
#endif
    return decode_csv_geofence_event_payload(payload);
}

} // namespace signalroute::proto_boundary
