#include "validator.h"
#include <chrono>
#include <cmath>

namespace signalroute {

Validator::Validator(const GatewayConfig& config) : config_(config) {}

Result<void, std::string> Validator::validate(const LocationEvent& event) const {
    if (event.device_id.empty()) {
        return Result<void, std::string>::err("device_id is required");
    }
    if (!std::isfinite(event.lat) || !std::isfinite(event.lon)) {
        return Result<void, std::string>::err("coordinates must be finite");
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
    if (event.accuracy_m < 0.0f) {
        return Result<void, std::string>::err("accuracy_m must be >= 0");
    }

    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    if (event.timestamp_ms <= 0) {
        return Result<void, std::string>::err("timestamp_ms is required");
    }
    if (event.timestamp_ms > now_ms + static_cast<int64_t>(config_.timestamp_skew_tolerance_s) * 1000) {
        return Result<void, std::string>::err("timestamp is in the future");
    }
    if (event.timestamp_ms < now_ms - 24LL * 60 * 60 * 1000) {
        return Result<void, std::string>::err("timestamp is too old");
    }

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
