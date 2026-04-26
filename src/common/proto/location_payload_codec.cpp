#include "location_payload_codec.h"

#if SIGNALROUTE_HAS_PROTOBUF
#include "generated_conversions.h"
#endif

#include <exception>
#include <sstream>
#include <string>
#include <vector>

namespace signalroute::proto_boundary {
namespace {

constexpr std::string_view kProtobufLocationPrefix = "SRPB1:";

std::vector<std::string> split_csv(const std::string& payload) {
    std::vector<std::string> fields;
    std::stringstream in(payload);
    std::string field;
    while (std::getline(in, field, ',')) {
        fields.push_back(field);
    }
    return fields;
}

std::string encode_csv_location_payload(const LocationEvent& event) {
    std::ostringstream out;
    out << event.device_id << ','
        << event.seq << ','
        << event.timestamp_ms << ','
        << event.server_recv_ms << ','
        << event.lat << ','
        << event.lon;
    return out.str();
}

Result<LocationEvent, std::string> decode_csv_location_payload(const std::string& payload) {
    const auto fields = split_csv(payload);
    if (fields.size() != 6) {
        return Result<LocationEvent, std::string>::err("invalid location payload");
    }

    try {
        LocationEvent event;
        event.device_id = fields[0];
        event.seq = static_cast<uint64_t>(std::stoull(fields[1]));
        event.timestamp_ms = std::stoll(fields[2]);
        event.server_recv_ms = std::stoll(fields[3]);
        event.lat = std::stod(fields[4]);
        event.lon = std::stod(fields[5]);
        if (event.device_id.empty()) {
            return Result<LocationEvent, std::string>::err("device_id is required");
        }
        return Result<LocationEvent, std::string>::ok(std::move(event));
    } catch (const std::exception&) {
        return Result<LocationEvent, std::string>::err("invalid location payload");
    }
}

bool has_prefix(const std::string& payload, std::string_view prefix) {
    return payload.size() >= prefix.size() &&
           payload.compare(0, prefix.size(), prefix.data(), prefix.size()) == 0;
}

} // namespace

std::string_view protobuf_location_payload_prefix() {
    return kProtobufLocationPrefix;
}

std::string encode_location_payload(const LocationEvent& event) {
#if SIGNALROUTE_HAS_PROTOBUF
    auto message = to_generated_location_event(event);
    std::string bytes;
    if (message.SerializeToString(&bytes)) {
        return std::string(kProtobufLocationPrefix) + bytes;
    }
#endif
    return encode_csv_location_payload(event);
}

Result<LocationEvent, std::string> decode_location_payload(const std::string& payload) {
#if SIGNALROUTE_HAS_PROTOBUF
    if (has_prefix(payload, kProtobufLocationPrefix)) {
        signalroute::v1::LocationEvent message;
        const std::string bytes = payload.substr(kProtobufLocationPrefix.size());
        if (!message.ParseFromString(bytes)) {
            return Result<LocationEvent, std::string>::err("invalid protobuf location payload");
        }
        return location_event_from_generated(message);
    }
#endif
    return decode_csv_location_payload(payload);
}

} // namespace signalroute::proto_boundary
