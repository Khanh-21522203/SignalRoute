#include "admission_control.h"

#include <utility>

namespace signalroute {

GatewayAdmissionLease::GatewayAdmissionLease(GatewayAdmissionControl* owner) : owner_(owner) {}

GatewayAdmissionLease::~GatewayAdmissionLease() {
    if (owner_) {
        owner_->release();
    }
}

GatewayAdmissionLease::GatewayAdmissionLease(GatewayAdmissionLease&& other) noexcept
    : owner_(std::exchange(other.owner_, nullptr)) {}

GatewayAdmissionLease& GatewayAdmissionLease::operator=(GatewayAdmissionLease&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    if (owner_) {
        owner_->release();
    }
    owner_ = std::exchange(other.owner_, nullptr);
    return *this;
}

bool GatewayAdmissionLease::active() const {
    return owner_ != nullptr;
}

GatewayAdmissionControl::GatewayAdmissionControl(GatewayConfig config) : config_(std::move(config)) {}

Result<void, std::string> GatewayAdmissionControl::authorize(const std::string& api_key) const {
    if (!config_.auth_required) {
        return Result<void, std::string>::ok();
    }
    if (!config_.api_key.empty() && api_key == config_.api_key) {
        return Result<void, std::string>::ok();
    }
    return Result<void, std::string>::err("unauthorized");
}

Result<GatewayAdmissionLease, std::string> GatewayAdmissionControl::try_acquire() {
    const int limit = config_.max_in_flight_requests;
    if (limit <= 0) {
        return Result<GatewayAdmissionLease, std::string>::ok(GatewayAdmissionLease());
    }

    int current = in_flight_.load();
    while (current < limit) {
        if (in_flight_.compare_exchange_weak(current, current + 1)) {
            return Result<GatewayAdmissionLease, std::string>::ok(GatewayAdmissionLease(this));
        }
    }
    return Result<GatewayAdmissionLease, std::string>::err("gateway backpressure");
}

int GatewayAdmissionControl::in_flight() const {
    return in_flight_.load();
}

void GatewayAdmissionControl::release() {
    in_flight_.fetch_sub(1);
}

} // namespace signalroute
