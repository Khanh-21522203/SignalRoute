#include "matching_payload_codec.h"

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

constexpr std::string_view kProtobufMatchRequestPrefix = "SRMRQ1:";
constexpr std::string_view kProtobufMatchResultPrefix = "SRMRS1:";

std::vector<std::string> split_csv(const std::string& payload, char delimiter = ',') {
    std::vector<std::string> fields;
    std::stringstream in(payload);
    std::string field;
    while (std::getline(in, field, delimiter)) {
        fields.push_back(field);
    }
    if (!payload.empty() && payload.back() == delimiter) {
        fields.emplace_back();
    }
    return fields;
}

std::string join_ids(const std::vector<std::string>& ids) {
    std::ostringstream out;
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (i != 0) {
            out << ';';
        }
        out << ids[i];
    }
    return out.str();
}

const char* match_status_to_string(MatchStatus status) {
    switch (status) {
        case MatchStatus::MATCHED:
            return "MATCHED";
        case MatchStatus::EXPIRED:
            return "EXPIRED";
        case MatchStatus::FAILED:
            return "FAILED";
    }
    return "FAILED";
}

MatchStatus match_status_from_string(const std::string& value) {
    if (value == "MATCHED") {
        return MatchStatus::MATCHED;
    }
    if (value == "EXPIRED") {
        return MatchStatus::EXPIRED;
    }
    return MatchStatus::FAILED;
}

std::string encode_csv_match_request_payload(const MatchRequest& request) {
    std::ostringstream out;
    out << request.request_id << ','
        << request.requester_id << ','
        << request.lat << ','
        << request.lon << ','
        << request.radius_m << ','
        << request.max_agents << ','
        << request.deadline_ms << ','
        << request.strategy;
    return out.str();
}

Result<MatchRequest, std::string> decode_csv_match_request_payload(const std::string& payload) {
    const auto fields = split_csv(payload);
    if (fields.size() != 8) {
        return Result<MatchRequest, std::string>::err("invalid match request payload");
    }

    try {
        MatchRequest request;
        request.request_id = fields[0];
        request.requester_id = fields[1];
        request.lat = std::stod(fields[2]);
        request.lon = std::stod(fields[3]);
        request.radius_m = std::stod(fields[4]);
        request.max_agents = std::stoi(fields[5]);
        request.deadline_ms = std::stoll(fields[6]);
        request.strategy = fields[7];
        if (request.request_id.empty()) {
            return Result<MatchRequest, std::string>::err("request_id is required");
        }
        return Result<MatchRequest, std::string>::ok(std::move(request));
    } catch (const std::exception&) {
        return Result<MatchRequest, std::string>::err("invalid match request payload");
    }
}

std::string encode_csv_match_result_payload(const MatchResult& result) {
    std::ostringstream out;
    out << result.request_id << ','
        << match_status_to_string(result.status) << ','
        << result.latency_ms << ','
        << result.reason << ','
        << join_ids(result.assigned_agent_ids);
    return out.str();
}

Result<MatchResult, std::string> decode_csv_match_result_payload(const std::string& payload) {
    const auto fields = split_csv(payload);
    if (fields.size() != 5) {
        return Result<MatchResult, std::string>::err("invalid match result payload");
    }

    try {
        MatchResult result;
        result.request_id = fields[0];
        result.status = match_status_from_string(fields[1]);
        result.latency_ms = std::stoll(fields[2]);
        result.reason = fields[3];
        if (!fields[4].empty()) {
            result.assigned_agent_ids = split_csv(fields[4], ';');
        }
        if (result.request_id.empty()) {
            return Result<MatchResult, std::string>::err("request_id is required");
        }
        return Result<MatchResult, std::string>::ok(std::move(result));
    } catch (const std::exception&) {
        return Result<MatchResult, std::string>::err("invalid match result payload");
    }
}

bool has_prefix(const std::string& payload, std::string_view prefix) {
    return payload.size() >= prefix.size() &&
           payload.compare(0, prefix.size(), prefix.data(), prefix.size()) == 0;
}

} // namespace

std::string_view protobuf_match_request_payload_prefix() {
    return kProtobufMatchRequestPrefix;
}

std::string encode_match_request_payload(const MatchRequest& request) {
#if SIGNALROUTE_HAS_PROTOBUF
    auto message = to_generated_match_request(request);
    std::string bytes;
    if (message.SerializeToString(&bytes)) {
        return std::string(kProtobufMatchRequestPrefix) + bytes;
    }
#endif
    return encode_csv_match_request_payload(request);
}

Result<MatchRequest, std::string> decode_match_request_payload(const std::string& payload) {
#if SIGNALROUTE_HAS_PROTOBUF
    if (has_prefix(payload, kProtobufMatchRequestPrefix)) {
        signalroute::v1::MatchRequest message;
        const std::string bytes = payload.substr(kProtobufMatchRequestPrefix.size());
        if (!message.ParseFromString(bytes)) {
            return Result<MatchRequest, std::string>::err("invalid protobuf match request payload");
        }
        return match_request_from_generated(message);
    }
#endif
    return decode_csv_match_request_payload(payload);
}

std::string_view protobuf_match_result_payload_prefix() {
    return kProtobufMatchResultPrefix;
}

std::string encode_match_result_payload(const MatchResult& result,
                                        const std::string& requester_id,
                                        const std::string& strategy) {
#if SIGNALROUTE_HAS_PROTOBUF
    auto message = to_generated_match_result(result, requester_id, strategy);
    std::string bytes;
    if (message.SerializeToString(&bytes)) {
        return std::string(kProtobufMatchResultPrefix) + bytes;
    }
#endif
    (void)requester_id;
    (void)strategy;
    return encode_csv_match_result_payload(result);
}

Result<MatchResult, std::string> decode_match_result_payload(const std::string& payload) {
#if SIGNALROUTE_HAS_PROTOBUF
    if (has_prefix(payload, kProtobufMatchResultPrefix)) {
        signalroute::v1::MatchResult message;
        const std::string bytes = payload.substr(kProtobufMatchResultPrefix.size());
        if (!message.ParseFromString(bytes)) {
            return Result<MatchResult, std::string>::err("invalid protobuf match result payload");
        }
        return match_result_from_generated(message);
    }
#endif
    return decode_csv_match_result_payload(payload);
}

} // namespace signalroute::proto_boundary
