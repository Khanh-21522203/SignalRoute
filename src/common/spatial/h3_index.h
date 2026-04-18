#pragma once

/**
 * SignalRoute — H3 Spatial Index
 *
 * Wraps the H3 C library to provide:
 *   - GPS coordinate → H3 cell encoding
 *   - k-ring (grid disk) expansion for nearby search
 *   - Radius-to-k conversion
 *   - Polygon polyfill for geofence coverage
 *
 * Dependencies: h3 (https://github.com/uber/h3)
 *
 * Thread-safe: all methods are const and stateless beyond the resolution.
 */

#include <vector>
#include <cstdint>
#include <utility>

namespace signalroute {

class H3Index {
public:
    /**
     * @param resolution H3 resolution level (0-15). Default: 7.
     *   Resolution 7 ≈ 5.2 km² per cell, ~1.4 km edge length.
     */
    explicit H3Index(int resolution = 7);

    /**
     * Encode a GPS coordinate to an H3 cell ID.
     *
     * @param lat WGS-84 latitude [-90, 90]
     * @param lon WGS-84 longitude [-180, 180]
     * @return H3 cell index as int64
     *
     * TODO: Implement using h3::latLngToCell(lat, lon, resolution_)
     */
    int64_t lat_lng_to_cell(double lat, double lon) const;

    /**
     * Compute the grid disk (k-ring): all cells within k steps of center.
     *
     * @param center_cell H3 cell ID
     * @param k Ring size (k=1 → 7 cells, k=2 → 19, k=4 → 61, etc.)
     * @return Vector of H3 cell IDs in the ring
     *
     * The result count is 3*k*(k+1)+1.
     *
     * TODO: Implement using h3::gridDisk(center_cell, k)
     */
    std::vector<int64_t> grid_disk(int64_t center_cell, int k) const;

    /**
     * Convert a search radius in meters to the appropriate k value.
     *
     * Formula: k = ceil(radius_m / avg_edge_length_m)
     *
     * @param radius_m Search radius in meters
     * @return k value for grid_disk
     *
     * TODO: Implement using h3::getHexagonEdgeLengthAvgM(resolution_)
     */
    int radius_to_k(double radius_m) const;

    /**
     * Compute the H3 polyfill: all cells covering a polygon.
     * Used for geofence H3 pre-filtering.
     *
     * @param polygon Vector of (lat, lon) vertices defining the polygon
     * @return Set of H3 cell IDs that cover the polygon
     *
     * Uses CONTAINMENT_OVERLAPPING mode to ensure no false negatives.
     *
     * TODO: Implement using h3::polygonToCells(polygon, resolution_)
     */
    std::vector<int64_t> polygon_to_cells(
        const std::vector<std::pair<double, double>>& polygon) const;

    /**
     * Get the average edge length at this resolution (meters).
     *
     * TODO: Implement using h3::getHexagonEdgeLengthAvgM(resolution_)
     */
    double avg_edge_length_m() const;

    /// Get the configured resolution.
    int resolution() const { return resolution_; }

private:
    int resolution_;
};

} // namespace signalroute
