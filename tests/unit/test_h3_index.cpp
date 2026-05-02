/**
 * SignalRoute - Unit Tests: H3 fallback index
 *
 * Standalone assert-based tests until the project adopts a test framework.
 */

#include "common/spatial/h3_index.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <vector>

#ifndef SIGNALROUTE_HAS_H3
#define SIGNALROUTE_HAS_H3 0
#endif

namespace {

template <typename Fn>
void expect_invalid_argument(Fn&& fn) {
    try {
        fn();
    } catch (const std::invalid_argument&) {
        return;
    }
    assert(false && "expected std::invalid_argument");
}

void test_resolution_validation() {
    signalroute::H3Index min_res(0);
    signalroute::H3Index max_res(15);

    assert(min_res.resolution() == 0);
    assert(max_res.resolution() == 15);
    expect_invalid_argument([] { signalroute::H3Index(-1); });
    expect_invalid_argument([] { signalroute::H3Index(16); });
    std::cout << "  PASS: resolution validation\n";
}

void test_coordinate_bounds() {
    signalroute::H3Index h3(7);

    (void)h3.lat_lng_to_cell(-90.0, -180.0);
    (void)h3.lat_lng_to_cell(90.0, 180.0);

    expect_invalid_argument([&] { h3.lat_lng_to_cell(-90.000001, 0.0); });
    expect_invalid_argument([&] { h3.lat_lng_to_cell(90.000001, 0.0); });
    expect_invalid_argument([&] { h3.lat_lng_to_cell(0.0, -180.000001); });
    expect_invalid_argument([&] { h3.lat_lng_to_cell(0.0, 180.000001); });
    expect_invalid_argument([&] {
        h3.lat_lng_to_cell(std::numeric_limits<double>::quiet_NaN(), 0.0);
    });
    expect_invalid_argument([&] {
        h3.lat_lng_to_cell(0.0, std::numeric_limits<double>::infinity());
    });
    std::cout << "  PASS: coordinate bounds\n";
}

void test_deterministic_cell_generation() {
    signalroute::H3Index h3(7);
    signalroute::H3Index h3_res8(8);

    const int64_t first = h3.lat_lng_to_cell(10.8231, 106.6297);
    const int64_t second = h3.lat_lng_to_cell(10.8231, 106.6297);
    const int64_t different_resolution = h3_res8.lat_lng_to_cell(10.8231, 106.6297);

    assert(first != 0);
    assert(first == second);
#if SIGNALROUTE_HAS_H3
    const int64_t nearby = h3.lat_lng_to_cell(10.8231001, 106.6297001);
    assert(nearby != 0);
#else
    const int64_t same_bucket = h3.lat_lng_to_cell(10.8231001, 106.6297001);
    assert(first == same_bucket);
#endif
    assert(first != different_resolution);
    std::cout << "  PASS: deterministic cell generation\n";
}

void test_grid_disk_counts_and_negative_k() {
    signalroute::H3Index h3(7);
    const int64_t center = h3.lat_lng_to_cell(10.8231, 106.6297);

    const auto disk0 = h3.grid_disk(center, 0);
    const auto disk1 = h3.grid_disk(center, 1);
    const auto disk2 = h3.grid_disk(center, 2);
    const auto negative = h3.grid_disk(center, -1);

    assert(disk0.size() == 1);
    assert(disk0.front() == center);
    assert(disk1.size() == 7);
    assert(disk2.size() == 19);
    assert(negative.empty());

    assert(std::set<int64_t>(disk1.begin(), disk1.end()).size() == disk1.size());
    assert(std::set<int64_t>(disk2.begin(), disk2.end()).size() == disk2.size());
    std::cout << "  PASS: grid_disk counts and negative k\n";
}

void test_radius_to_k() {
    signalroute::H3Index h3(7);
    const double edge = h3.avg_edge_length_m();

    assert(h3.radius_to_k(0.0) == 0);
    assert(h3.radius_to_k(1.0) == 1);
    assert(h3.radius_to_k(edge) == 1);
    assert(h3.radius_to_k(edge + 0.001) == 2);
    assert(h3.radius_to_k(edge * 100.0) == 100);

    expect_invalid_argument([&] { h3.radius_to_k(-1.0); });
    expect_invalid_argument([&] {
        h3.radius_to_k(std::numeric_limits<double>::infinity());
    });
    std::cout << "  PASS: radius_to_k\n";
}

void test_polygon_to_cells_deduplication() {
    signalroute::H3Index h3(7);
#if SIGNALROUTE_HAS_H3
    const std::vector<std::pair<double, double>> polygon = {
        {10.8100, 106.6100},
        {10.8100, 106.6500},
        {10.8500, 106.6500},
        {10.8500, 106.6100},
    };
#else
    const std::vector<std::pair<double, double>> polygon = {
        {10.8231, 106.6297},
        {10.8231001, 106.6297001},
        {10.8241, 106.6307},
        {10.8231, 106.6297},
        {10.8241, 106.6307},
    };
#endif

    const auto cells = h3.polygon_to_cells(polygon);
    const std::set<int64_t> unique(cells.begin(), cells.end());

    assert(!cells.empty());
    assert(cells.size() == unique.size());
#if !SIGNALROUTE_HAS_H3
    assert(cells.size() == 1);
#endif
    std::cout << "  PASS: polygon_to_cells deduplication\n";
}

void test_polygon_validation_and_empty_input() {
    signalroute::H3Index h3(7);

    assert(h3.polygon_to_cells({}).empty());
    expect_invalid_argument([&] {
        h3.polygon_to_cells({{10.0, 106.0}, {std::numeric_limits<double>::infinity(), 106.0}});
    });
    expect_invalid_argument([&] {
        h3.polygon_to_cells({{10.0, 106.0}, {10.0, 180.000001}});
    });
    std::cout << "  PASS: polygon validation and empty input\n";
}

void test_average_edge_length_decreases_with_resolution() {
    double previous = signalroute::H3Index(0).avg_edge_length_m();
    for (int resolution = 1; resolution <= 15; ++resolution) {
        const double current = signalroute::H3Index(resolution).avg_edge_length_m();
        assert(current > 0.0);
        assert(current < previous);
        previous = current;
    }
    std::cout << "  PASS: average edge length by resolution\n";
}

} // namespace

int main() {
    std::cout << "test_h3_index:\n";
    test_resolution_validation();
    test_coordinate_bounds();
    test_deterministic_cell_generation();
    test_grid_disk_counts_and_negative_k();
    test_radius_to_k();
    test_polygon_to_cells_deduplication();
    test_polygon_validation_and_empty_input();
    test_average_edge_length_decreases_with_resolution();
    std::cout << "All H3 index tests passed.\n";
    return 0;
}
