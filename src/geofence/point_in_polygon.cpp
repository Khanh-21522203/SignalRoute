#include "point_in_polygon.h"

#include <algorithm>
#include <cmath>

namespace signalroute::geo {

namespace {

constexpr double kEpsilon = 1e-12;

bool same_point(const std::pair<double, double>& a,
                const std::pair<double, double>& b) {
    return std::abs(a.first - b.first) <= kEpsilon &&
           std::abs(a.second - b.second) <= kEpsilon;
}

double cross_product(double lat, double lon,
                     const std::pair<double, double>& a,
                     const std::pair<double, double>& b) {
    const double y1 = a.first;
    const double x1 = a.second;
    const double y2 = b.first;
    const double x2 = b.second;
    return (x2 - x1) * (lat - y1) - (lon - x1) * (y2 - y1);
}

bool point_on_segment(double lat, double lon,
                      const std::pair<double, double>& a,
                      const std::pair<double, double>& b) {
    if (std::abs(cross_product(lat, lon, a, b)) > kEpsilon) {
        return false;
    }

    const double min_lat = std::min(a.first, b.first) - kEpsilon;
    const double max_lat = std::max(a.first, b.first) + kEpsilon;
    const double min_lon = std::min(a.second, b.second) - kEpsilon;
    const double max_lon = std::max(a.second, b.second) + kEpsilon;
    return lat >= min_lat && lat <= max_lat &&
           lon >= min_lon && lon <= max_lon;
}

double signed_area_twice(const std::vector<std::pair<double, double>>& vertices,
                         size_t n) {
    double area = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const auto& current = vertices[i];
        const auto& next = vertices[(i + 1) % n];
        area += current.second * next.first - next.second * current.first;
    }
    return area;
}

} // namespace

bool point_in_polygon(double lat, double lon,
                      const std::vector<std::pair<double, double>>& vertices) {
    if (vertices.size() < 3) return false;

    size_t n = vertices.size();
    if (same_point(vertices.front(), vertices.back())) {
        --n;
    }
    if (n < 3) return false;
    if (std::abs(signed_area_twice(vertices, n)) <= kEpsilon) return false;

    // Winding number algorithm
    int winding = 0;

    for (size_t i = 0; i < n; ++i) {
        double y1 = vertices[i].first;       // lat
        double y2 = vertices[(i + 1) % n].first;

        if (point_on_segment(lat, lon, vertices[i], vertices[(i + 1) % n])) {
            return true;
        }

        if (y1 <= lat) {
            if (y2 > lat) {
                // Upward crossing
                double cross = cross_product(lat, lon, vertices[i], vertices[(i + 1) % n]);
                if (cross > 0) ++winding;
            }
        } else {
            if (y2 <= lat) {
                // Downward crossing
                double cross = cross_product(lat, lon, vertices[i], vertices[(i + 1) % n]);
                if (cross < 0) --winding;
            }
        }
    }

    return winding != 0;
}

} // namespace signalroute::geo
