/**
 * SignalRoute — Unit Tests: Validator
 */

#include "gateway/validator.h"
#include <cassert>
#include <iostream>
#include <chrono>

int64_t now_ms() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

void test_valid_event() {
    signalroute::GatewayConfig cfg;
    cfg.max_batch_events = 100;
    cfg.timestamp_skew_tolerance_s = 60;
    signalroute::Validator v(cfg);

    signalroute::LocationEvent e;
    e.device_id = "device-1";
    e.lat = 10.8231;
    e.lon = 106.6297;
    e.timestamp_ms = now_ms();
    e.seq = 42;

    auto result = v.validate(e);
    assert(result.is_ok());
    std::cout << "  PASS: valid event\n";
}

void test_invalid_coords() {
    signalroute::GatewayConfig cfg;
    cfg.max_batch_events = 100;
    signalroute::Validator v(cfg);

    signalroute::LocationEvent e;
    e.device_id = "dev-1";
    e.lat = 91.0;  // Out of range
    e.lon = 106.0;
    e.timestamp_ms = now_ms();
    e.seq = 1;

    auto result = v.validate(e);
    assert(!result.is_ok());
    std::cout << "  PASS: invalid latitude rejected\n";

    e.lat = 10.0;
    e.lon = 181.0;  // Out of range
    result = v.validate(e);
    assert(!result.is_ok());
    std::cout << "  PASS: invalid longitude rejected\n";
}

void test_missing_device_id() {
    signalroute::GatewayConfig cfg;
    cfg.max_batch_events = 100;
    signalroute::Validator v(cfg);

    signalroute::LocationEvent e;
    e.device_id = "";
    e.lat = 10.0;
    e.lon = 106.0;
    e.timestamp_ms = now_ms();
    e.seq = 1;

    auto result = v.validate(e);
    assert(!result.is_ok());
    std::cout << "  PASS: empty device_id rejected\n";
}

void test_batch_size_limit() {
    signalroute::GatewayConfig cfg;
    cfg.max_batch_events = 2;
    signalroute::Validator v(cfg);

    signalroute::LocationEvent e;
    e.device_id = "dev-1";
    e.lat = 10.0;
    e.lon = 106.0;
    e.timestamp_ms = now_ms();
    e.seq = 1;

    auto results = v.validate_batch({e, e, e});  // 3 > max 2
    assert(results.size() == 3);
    for (auto& r : results) {
        assert(!r.is_ok());
    }
    std::cout << "  PASS: batch size limit enforced\n";
}

int main() {
    std::cout << "test_validator:\n";
    test_valid_event();
    test_invalid_coords();
    test_missing_device_id();
    test_batch_size_limit();
    std::cout << "All validator tests passed.\n";
    return 0;
}
