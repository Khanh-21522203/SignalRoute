#include "validator.h"
#include <chrono>

namespace signalroute {

Validator::Validator(const GatewayConfig& config) : config_(config) {}

Result<void, std::string> Validator::validate(const LocationEvent& event) const {
    // TODO: Implement all validation checks

    if (event.device_id.empty()) {
        return Result<void, std::string>::err("device_id is required");
    }
    if (event.lat < -90.0 || event.lat > 90.0) {
        return Result<void, std::string>::err("lat out of range [-90, 90]");
    }
    if (event.lon < -180.0 || event.lon > 180.0) {
        return Result<void, std::string>::err("lon out of range [-180, 180]");
    }
    if (event.seq == 0) {
        return Result<void, std::string>::err("seq must be > 0");
    }

    // TODO: Timestamp validation
    //   auto now = std::chrono::system_clock::now().time_since_epoch();
    //   auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    //   if (event.timestamp_ms > now_ms + config_.timestamp_skew_tolerance_s * 1000) {
    //       return Result<void, std::string>::err("timestamp is in the future");
    //   }

    return Result<void, std::string>::ok();
}

std::vector<Result<void, std::string>> Validator::validate_batch(
    const std::vector<LocationEvent>& events) const
{
    std::vector<Result<void, std::string>> results;
    results.reserve(events.size());

    if (static_cast<int>(events.size()) > config_.max_batch_events) {
        // Reject entire batch if over limit
        for (size_t i = 0; i < events.size(); ++i) {
            results.push_back(Result<void, std::string>::err("batch too large"));
        }
        return results;
    }

    for (const auto& event : events) {
        results.push_back(validate(event));
    }
    return results;
}

} // namespace signalroute
