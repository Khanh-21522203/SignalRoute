#pragma once

/**
 * SignalRoute — Point-in-Polygon Test
 *
 * Winding number algorithm for determining if a point lies inside
 * a polygon (works for both convex and simple concave polygons).
 *
 * Policy:
 * - Open and closed rings are accepted; duplicate closing vertex is ignored.
 * - Clockwise and counter-clockwise vertex order are equivalent.
 * - Points on an edge or vertex are treated as inside.
 * - Degenerate polygons (fewer than 3 effective vertices or zero area)
 *   never contain points.
 *
 * For complex polygons with holes, fall back to PostGIS ST_Contains.
 */

#include <vector>
#include <utility>

namespace signalroute::geo {

/**
 * Winding number test for point-in-polygon containment.
 *
 * @param lat Point latitude
 * @param lon Point longitude
 * @param vertices Polygon vertices as (lat, lon) pairs.
 *                 Must be a closed ring (first == last) or will be auto-closed.
 * @return true if point is inside the polygon or on its boundary
 *
 * Time complexity: O(n) where n = number of vertices
 * Thread-safe: pure function
 */
bool point_in_polygon(double lat, double lon,
                      const std::vector<std::pair<double, double>>& vertices);

} // namespace signalroute::geo
