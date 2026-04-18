/**
 * SignalRoute — Unit Tests: Haversine Distance
 *
 * TODO: Implement using Google Test or Catch2
 */

// #include <gtest/gtest.h>
#include "../src/common/spatial/haversine.h"
#include <cassert>
#include <cmath>
#include <iostream>

void test_haversine_zero_distance() {
    double d = signalroute::geo::haversine(0.0, 0.0, 0.0, 0.0);
    assert(d < 0.001);
    std::cout << "  PASS: zero distance\n";
}

void test_haversine_known_distance() {
    // Ho Chi Minh City (10.8231, 106.6297) → Hanoi (21.0285, 105.8542)
    // Expected: ~1138 km
    double d = signalroute::geo::haversine(10.8231, 106.6297, 21.0285, 105.8542);
    assert(d > 1100000.0 && d < 1200000.0);
    std::cout << "  PASS: HCMC-Hanoi = " << d / 1000.0 << " km\n";
}

void test_haversine_antipodal() {
    // North pole to south pole ≈ 20015 km
    double d = signalroute::geo::haversine(90.0, 0.0, -90.0, 0.0);
    assert(d > 20000000.0 && d < 20100000.0);
    std::cout << "  PASS: pole-to-pole = " << d / 1000.0 << " km\n";
}

int main() {
    std::cout << "test_haversine:\n";
    test_haversine_zero_distance();
    test_haversine_known_distance();
    test_haversine_antipodal();
    std::cout << "All haversine tests passed.\n";
    return 0;
}
