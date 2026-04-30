#include "gateway/admission_control.h"

#include <cassert>
#include <iostream>

void test_auth_allows_when_disabled() {
    signalroute::GatewayConfig config;
    config.auth_required = false;
    signalroute::GatewayAdmissionControl admission(config);

    assert(admission.authorize("").is_ok());
    assert(admission.authorize("anything").is_ok());
}

void test_auth_requires_matching_api_key() {
    signalroute::GatewayConfig config;
    config.auth_required = true;
    config.api_key = "secret";
    signalroute::GatewayAdmissionControl admission(config);

    assert(admission.authorize("").is_err());
    assert(admission.authorize("wrong").is_err());
    assert(admission.authorize("secret").is_ok());
}

void test_unlimited_admission_does_not_track_in_flight() {
    signalroute::GatewayConfig config;
    config.max_in_flight_requests = 0;
    signalroute::GatewayAdmissionControl admission(config);

    auto first = admission.try_acquire();
    auto second = admission.try_acquire();

    assert(first.is_ok());
    assert(second.is_ok());
    assert(admission.in_flight() == 0);
}

void test_bounded_admission_rejects_when_full_and_releases_on_scope_exit() {
    signalroute::GatewayConfig config;
    config.max_in_flight_requests = 1;
    signalroute::GatewayAdmissionControl admission(config);

    {
        auto first = admission.try_acquire();
        assert(first.is_ok());
        assert(first.value().active());
        assert(admission.in_flight() == 1);

        auto second = admission.try_acquire();
        assert(second.is_err());
        assert(second.error() == "gateway backpressure");
        assert(admission.in_flight() == 1);
    }

    assert(admission.in_flight() == 0);
    assert(admission.try_acquire().is_ok());
}

int main() {
    std::cout << "test_gateway_admission_control:\n";
    test_auth_allows_when_disabled();
    test_auth_requires_matching_api_key();
    test_unlimited_admission_does_not_track_in_flight();
    test_bounded_admission_rejects_when_full_and_releases_on_scope_exit();
    std::cout << "All gateway admission control tests passed.\n";
    return 0;
}
