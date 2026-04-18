#include "h3_index.h"

#include <cmath>
#include <stdexcept>

// TODO: #include <h3/h3api.h>  // H3 C API

namespace signalroute {

H3Index::H3Index(int resolution)
    : resolution_(resolution)
{
    if (resolution < 0 || resolution > 15) {
        throw std::invalid_argument("H3 resolution must be 0-15, got: "
                                    + std::to_string(resolution));
    }
}

int64_t H3Index::lat_lng_to_cell(double lat, double lon) const {
    // TODO: Implement using H3 C API
    //
    //   LatLng coord;
    //   coord.lat = degsToRads(lat);
    //   coord.lng = degsToRads(lon);
    //   H3Index cell;
    //   if (latLngToCell(&coord, resolution_, &cell) != E_SUCCESS) {
    //       throw std::runtime_error("H3 latLngToCell failed");
    //   }
    //   return static_cast<int64_t>(cell);

    return 0; // placeholder
}

std::vector<int64_t> H3Index::grid_disk(int64_t center_cell, int k) const {
    // TODO: Implement using H3 C API
    //
    //   int64_t max_size;
    //   maxGridDiskSize(k, &max_size);
    //   std::vector<H3Index> out(max_size);
    //   gridDisk(static_cast<H3Index>(center_cell), k, out.data());
    //   // Filter out 0 (invalid) entries and cast to int64_t
    //   std::vector<int64_t> result;
    //   for (auto cell : out) {
    //       if (cell != 0) result.push_back(static_cast<int64_t>(cell));
    //   }
    //   return result;

    return {}; // placeholder
}

int H3Index::radius_to_k(double radius_m) const {
    double edge = avg_edge_length_m();
    if (edge <= 0) return 1;
    return static_cast<int>(std::ceil(radius_m / edge));
}

std::vector<int64_t> H3Index::polygon_to_cells(
    const std::vector<std::pair<double, double>>& polygon) const
{
    // TODO: Implement using H3 C API polygonToCells
    //
    //   1. Convert polygon vertices to H3 GeoLoop (LatLng array)
    //   2. Create GeoPolygon struct
    //   3. Call polygonToCells with CONTAINMENT_OVERLAPPING flag
    //   4. Convert result to vector<int64_t>

    return {}; // placeholder
}

double H3Index::avg_edge_length_m() const {
    // TODO: Implement using h3::getHexagonEdgeLengthAvgM(resolution_)
    //
    // Hardcoded approximate values for common resolutions:
    static const double edge_lengths[] = {
        1107712.591, // res 0
        418676.005,  // res 1
        158244.655,  // res 2
        59810.858,   // res 3
        22606.379,   // res 4
        8544.408,    // res 5
        3229.482,    // res 6
        1220.629,    // res 7
        461.354,     // res 8
        174.375,     // res 9
        65.907,      // res 10
        24.910,      // res 11
        9.415,       // res 12
        3.559,       // res 13
        1.348,       // res 14
        0.509        // res 15
    };
    if (resolution_ >= 0 && resolution_ <= 15) {
        return edge_lengths[resolution_];
    }
    return 1220.629; // fallback to res 7
}

} // namespace signalroute
