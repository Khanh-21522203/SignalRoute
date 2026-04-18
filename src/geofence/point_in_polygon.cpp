#include "point_in_polygon.h"

namespace signalroute::geo {

bool point_in_polygon(double lat, double lon,
                      const std::vector<std::pair<double, double>>& vertices) {
    if (vertices.size() < 3) return false;

    // Winding number algorithm
    int winding = 0;
    size_t n = vertices.size();

    for (size_t i = 0; i < n; ++i) {
        double y1 = vertices[i].first;       // lat
        double x1 = vertices[i].second;      // lon
        double y2 = vertices[(i + 1) % n].first;
        double x2 = vertices[(i + 1) % n].second;

        if (y1 <= lat) {
            if (y2 > lat) {
                // Upward crossing
                double cross = (x2 - x1) * (lat - y1) - (lon - x1) * (y2 - y1);
                if (cross > 0) ++winding;
            }
        } else {
            if (y2 <= lat) {
                // Downward crossing
                double cross = (x2 - x1) * (lat - y1) - (lon - x1) * (y2 - y1);
                if (cross < 0) --winding;
            }
        }
    }

    return winding != 0;
}

} // namespace signalroute::geo
