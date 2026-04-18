#pragma once

/**
 * SignalRoute — Haversine Distance Calculation
 *
 * Great-circle distance between two WGS-84 points using the haversine formula.
 * Accuracy: < 0.3% error worldwide. For distances < 50 km, error < 30 m.
 *
 * Thread-safe: pure functions, no state.
 */

namespace signalroute::geo {

/// Earth's mean radius in meters (WGS-84 approximation).
constexpr double EARTH_RADIUS_M = 6'371'000.0;

/**
 * Compute the great-circle distance between two points.
 *
 * @param lat1, lon1 First point (WGS-84 degrees)
 * @param lat2, lon2 Second point (WGS-84 degrees)
 * @return Distance in meters
 *
 * Formula:
 *   a = sin²(Δlat/2) + cos(lat1) × cos(lat2) × sin²(Δlon/2)
 *   c = 2 × atan2(√a, √(1−a))
 *   d = R × c
 */
double haversine(double lat1, double lon1, double lat2, double lon2);

/**
 * Convert degrees to radians.
 */
constexpr double deg_to_rad(double deg) {
    return deg * 3.14159265358979323846 / 180.0;
}

} // namespace signalroute::geo
