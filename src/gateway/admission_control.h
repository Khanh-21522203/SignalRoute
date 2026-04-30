#pragma once

#include "../common/config/config.h"
#include "../common/types/result.h"

#include <atomic>
#include <string>

namespace signalroute {

class GatewayAdmissionControl;

class GatewayAdmissionLease {
public:
    GatewayAdmissionLease() = default;
    ~GatewayAdmissionLease();

    GatewayAdmissionLease(const GatewayAdmissionLease&) = delete;
    GatewayAdmissionLease& operator=(const GatewayAdmissionLease&) = delete;

    GatewayAdmissionLease(GatewayAdmissionLease&& other) noexcept;
    GatewayAdmissionLease& operator=(GatewayAdmissionLease&& other) noexcept;

    [[nodiscard]] bool active() const;

private:
    friend class GatewayAdmissionControl;
    explicit GatewayAdmissionLease(GatewayAdmissionControl* owner);

    GatewayAdmissionControl* owner_ = nullptr;
};

class GatewayAdmissionControl {
public:
    explicit GatewayAdmissionControl(GatewayConfig config);

    [[nodiscard]] Result<void, std::string> authorize(const std::string& api_key) const;
    [[nodiscard]] Result<GatewayAdmissionLease, std::string> try_acquire();
    [[nodiscard]] int in_flight() const;

private:
    friend class GatewayAdmissionLease;
    void release();

    GatewayConfig config_;
    std::atomic<int> in_flight_{0};
};

} // namespace signalroute
