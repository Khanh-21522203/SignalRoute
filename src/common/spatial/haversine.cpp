#include "haversine.h"

#include <cmath>

namespace signalroute::geo {

double haversine(double lat1, double lon1, double lat2, double lon2) {
    // TODO: Consider SIMD vectorization for batch distance calculations
    //       (e.g., computing distance for 1000 candidates in ~50 μs)

    const double dlat = deg_to_rad(lat2 - lat1);
    const double dlon = deg_to_rad(lon2 - lon1);

    const double rlat1 = deg_to_rad(lat1);
    const double rlat2 = deg_to_rad(lat2);

    const double a = std::sin(dlat / 2.0) * std::sin(dlat / 2.0)
                   + std::cos(rlat1) * std::cos(rlat2)
                   * std::sin(dlon / 2.0) * std::sin(dlon / 2.0);

    const double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));

    return EARTH_RADIUS_M * c;
}

} // namespace signalroute::geo
