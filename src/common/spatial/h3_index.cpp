#include "h3_index.h"

#if SIGNALROUTE_HAS_H3
#if __has_include(<h3/h3api.h>)
#include <h3/h3api.h>
#elif __has_include(<h3api.h>)
#include <h3api.h>
#else
#error "SIGNALROUTE_HAS_H3 is enabled, but h3api.h was not found"
#endif
#endif

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace signalroute {

namespace {

constexpr int64_t kCoordMask = (1LL << 29) - 1;
constexpr int64_t kLatShift = 29;
constexpr int64_t kResShift = 58;

#if SIGNALROUTE_HAS_H3
constexpr double kPi = 3.141592653589793238462643383279502884;

double degs_to_rads(double degrees) {
    return degrees * kPi / 180.0;
}

LatLng to_h3_lat_lng(double lat, double lon) {
    LatLng out;
    out.lat = degs_to_rads(lat);
    out.lng = degs_to_rads(lon);
    return out;
}

void throw_on_h3_error(H3Error error, const std::string& operation) {
    if (error != E_SUCCESS) {
        throw std::runtime_error(operation + " failed with H3 error: " + std::to_string(error));
    }
}
#else
int64_t cells_per_degree(double edge_m) {
    return std::max<int64_t>(1, static_cast<int64_t>(std::llround(111'320.0 / edge_m)));
}

int64_t pack_cell(int resolution, int64_t lat_idx, int64_t lon_idx) {
    return (static_cast<int64_t>(resolution) << kResShift)
        | ((lat_idx & kCoordMask) << kLatShift)
        | (lon_idx & kCoordMask);
}

void unpack_cell(int64_t cell, int& resolution, int64_t& lat_idx, int64_t& lon_idx) {
    resolution = static_cast<int>((cell >> kResShift) & 0xF);
    lat_idx = (cell >> kLatShift) & kCoordMask;
    lon_idx = cell & kCoordMask;
}
#endif

bool valid_lat_lon(double lat, double lon) {
    return std::isfinite(lat) && std::isfinite(lon)
        && lat >= -90.0 && lat <= 90.0
        && lon >= -180.0 && lon <= 180.0;
}

void validate_lat_lon(double lat, double lon) {
    if (!valid_lat_lon(lat, lon)) {
        throw std::invalid_argument("coordinate out of range");
    }
}

} // namespace

H3Index::H3Index(int resolution)
    : resolution_(resolution)
{
    if (resolution < 0 || resolution > 15) {
        throw std::invalid_argument("H3 resolution must be 0-15, got: "
                                    + std::to_string(resolution));
    }
}

int64_t H3Index::lat_lng_to_cell(double lat, double lon) const {
    validate_lat_lon(lat, lon);

#if SIGNALROUTE_HAS_H3
    const LatLng geo = to_h3_lat_lng(lat, lon);
    ::H3Index cell = 0;
    throw_on_h3_error(latLngToCell(&geo, resolution_, &cell), "latLngToCell");
    return static_cast<int64_t>(cell);
#else
    const int64_t scale = cells_per_degree(avg_edge_length_m());
    const int64_t lat_idx = static_cast<int64_t>(std::floor((lat + 90.0) * scale));
    const int64_t lon_idx = static_cast<int64_t>(std::floor((lon + 180.0) * scale));
    return pack_cell(resolution_, lat_idx, lon_idx);
#endif
}

std::vector<int64_t> H3Index::grid_disk(int64_t center_cell, int k) const {
    if (k < 0) {
        return {};
    }

#if SIGNALROUTE_HAS_H3
    int64_t max_size = 0;
    throw_on_h3_error(maxGridDiskSize(k, &max_size), "maxGridDiskSize");

    std::vector<::H3Index> raw(static_cast<std::size_t>(max_size));
    throw_on_h3_error(gridDisk(static_cast<::H3Index>(center_cell), k, raw.data()), "gridDisk");

    std::vector<int64_t> result;
    result.reserve(raw.size());
    for (const auto cell : raw) {
        if (cell != 0) {
            result.push_back(static_cast<int64_t>(cell));
        }
    }
    return result;
#else
    int encoded_resolution = 0;
    int64_t center_lat = 0;
    int64_t center_lon = 0;
    unpack_cell(center_cell, encoded_resolution, center_lat, center_lon);

    std::vector<int64_t> result;
    result.reserve(static_cast<size_t>(3 * k * (k + 1) + 1));
    for (int dx = -k; dx <= k; ++dx) {
        for (int dy = std::max(-k, -dx - k); dy <= std::min(k, -dx + k); ++dy) {
            result.push_back(pack_cell(encoded_resolution, center_lat + dx, center_lon + dy));
        }
    }
    return result;
#endif
}

int H3Index::radius_to_k(double radius_m) const {
    if (!std::isfinite(radius_m) || radius_m < 0.0) {
        throw std::invalid_argument("radius must be finite and non-negative");
    }

    double edge = avg_edge_length_m();
    if (edge <= 0) return 1;
    return static_cast<int>(std::ceil(radius_m / edge));
}

std::vector<int64_t> H3Index::polygon_to_cells(
    const std::vector<std::pair<double, double>>& polygon) const
{
    if (polygon.empty()) {
        return {};
    }
    for (const auto& [lat, lon] : polygon) {
        validate_lat_lon(lat, lon);
    }

#if SIGNALROUTE_HAS_H3
    std::vector<LatLng> vertices;
    vertices.reserve(polygon.size());
    for (const auto& [lat, lon] : polygon) {
        vertices.push_back(to_h3_lat_lng(lat, lon));
    }

    GeoLoop loop;
    loop.numVerts = static_cast<int>(vertices.size());
    loop.verts = vertices.data();

    GeoPolygon geo_polygon;
    geo_polygon.geoloop = loop;
    geo_polygon.numHoles = 0;
    geo_polygon.holes = nullptr;

    int64_t max_size = 0;
    throw_on_h3_error(
        maxPolygonToCellsSize(&geo_polygon, resolution_, 0, &max_size),
        "maxPolygonToCellsSize");

    std::vector<::H3Index> raw(static_cast<std::size_t>(max_size));
    throw_on_h3_error(
        polygonToCells(&geo_polygon, resolution_, 0, raw.data()),
        "polygonToCells");

    std::vector<int64_t> cells;
    cells.reserve(raw.size());
    for (const auto cell : raw) {
        if (cell != 0) {
            cells.push_back(static_cast<int64_t>(cell));
        }
    }
    std::sort(cells.begin(), cells.end());
    cells.erase(std::unique(cells.begin(), cells.end()), cells.end());
    return cells;
#else
    std::vector<int64_t> cells;
    cells.reserve(polygon.size());
    for (const auto& [lat, lon] : polygon) {
        cells.push_back(lat_lng_to_cell(lat, lon));
    }
    std::sort(cells.begin(), cells.end());
    cells.erase(std::unique(cells.begin(), cells.end()), cells.end());
    return cells;
#endif
}

double H3Index::avg_edge_length_m() const {
#if SIGNALROUTE_HAS_H3
    double edge_m = 0.0;
    throw_on_h3_error(getHexagonEdgeLengthAvgM(resolution_, &edge_m), "getHexagonEdgeLengthAvgM");
    return edge_m;
#else
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
    return 1220.629;
#endif
}

} // namespace signalroute
