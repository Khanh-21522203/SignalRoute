#pragma once

#include "../types/matching_types.h"
#include "../types/result.h"

#include <string>
#include <string_view>

namespace signalroute::proto_boundary {

std::string encode_match_request_payload(const MatchRequest& request);
Result<MatchRequest, std::string> decode_match_request_payload(const std::string& payload);
std::string_view protobuf_match_request_payload_prefix();

std::string encode_match_result_payload(const MatchResult& result,
                                        const std::string& requester_id = {},
                                        const std::string& strategy = {});
Result<MatchResult, std::string> decode_match_result_payload(const std::string& payload);
std::string_view protobuf_match_result_payload_prefix();

} // namespace signalroute::proto_boundary
