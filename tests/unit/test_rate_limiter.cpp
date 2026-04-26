#include "gateway/rate_limiter.h"

#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

void test_allows_up_to_limit_per_window() {
    signalroute::RateLimiter limiter(2);

    assert(limiter.allow("dev-1"));
    assert(limiter.allow("dev-1"));
    assert(!limiter.allow("dev-1"));
    assert(limiter.current_rate("dev-1") == 2.0);
    assert(limiter.tracked_devices() == 1);
}

void test_devices_are_isolated_and_window_resets() {
    signalroute::RateLimiter limiter(1);

    assert(limiter.allow("dev-1"));
    assert(!limiter.allow("dev-1"));
    assert(limiter.allow("dev-2"));

    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    assert(limiter.allow("dev-1"));
}

void test_invalid_inputs_are_rejected() {
    signalroute::RateLimiter disabled(0);
    signalroute::RateLimiter limiter(1);

    assert(!disabled.allow("dev-1"));
    assert(!limiter.allow(""));
}

int main() {
    std::cout << "test_rate_limiter:\n";
    test_allows_up_to_limit_per_window();
    test_devices_are_isolated_and_window_resets();
    test_invalid_inputs_are_rejected();
    std::cout << "All rate limiter tests passed.\n";
    return 0;
}
