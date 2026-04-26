#include "rate_limiter.h"

namespace signalroute {
namespace {

constexpr auto kWindow = std::chrono::seconds(1);

} // namespace

RateLimiter::RateLimiter(int max_rps_per_device) : max_rps_(max_rps_per_device) {
}

bool RateLimiter::allow(const std::string& device_id) {
    if (device_id.empty() || max_rps_ <= 0) {
        return false;
    }

    const auto now = std::chrono::steady_clock::now();
    std::lock_guard lock(mu_);
    auto& window = windows_[device_id];
    if (window.count == 0 || now - window.started_at >= kWindow) {
        window.started_at = now;
        window.count = 0;
    }
    if (window.count >= max_rps_) {
        return false;
    }
    ++window.count;
    return true;
}

double RateLimiter::current_rate(const std::string& device_id) const {
    std::lock_guard lock(mu_);
    auto it = windows_.find(device_id);
    if (it == windows_.end()) {
        return 0.0;
    }
    return static_cast<double>(it->second.count);
}

size_t RateLimiter::tracked_devices() const {
    std::lock_guard lock(mu_);
    return windows_.size();
}

} // namespace signalroute
