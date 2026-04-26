/**
 * SignalRoute — Unit Tests: Point-in-Polygon
 *
 * TODO: Integrate with Google Test or Catch2
 */

#include "geofence/point_in_polygon.h"
#include <cassert>
#include <iostream>

using signalroute::geo::point_in_polygon;

void test_pip_open_and_closed_rings() {
    const std::vector<std::pair<double, double>> open_square = {
        {0, 0}, {0, 10}, {10, 10}, {10, 0}
    };
    const std::vector<std::pair<double, double>> closed_square = {
        {0, 0}, {0, 10}, {10, 10}, {10, 0}, {0, 0}
    };

    assert(point_in_polygon(5.0, 5.0, open_square) == true);
    assert(point_in_polygon(15.0, 5.0, open_square) == false);
    assert(point_in_polygon(-1.0, 5.0, open_square) == false);
    assert(point_in_polygon(5.0, 5.0, closed_square) == true);
    assert(point_in_polygon(15.0, 5.0, closed_square) == false);
    std::cout << "  PASS: open and closed rings\n";
}

void test_pip_vertex_order() {
    const std::vector<std::pair<double, double>> ccw_square = {
        {0, 0}, {0, 10}, {10, 10}, {10, 0}
    };
    const std::vector<std::pair<double, double>> cw_square = {
        {0, 0}, {10, 0}, {10, 10}, {0, 10}
    };

    assert(point_in_polygon(5.0, 5.0, ccw_square) == true);
    assert(point_in_polygon(5.0, 5.0, cw_square) == true);
    assert(point_in_polygon(15.0, 5.0, ccw_square) == false);
    assert(point_in_polygon(15.0, 5.0, cw_square) == false);
    std::cout << "  PASS: vertex order\n";
}

void test_pip_concave_polygon() {
    const std::vector<std::pair<double, double>> concave = {
        {0, 0}, {0, 10}, {5, 5}, {10, 10}, {10, 0}
    };

    assert(point_in_polygon(2.0, 2.0, concave) == true);
    assert(point_in_polygon(8.0, 2.0, concave) == true);
    assert(point_in_polygon(5.0, 7.0, concave) == false);
    assert(point_in_polygon(11.0, 5.0, concave) == false);
    std::cout << "  PASS: concave polygon\n";
}

void test_pip_boundary_points_are_inside() {
    const std::vector<std::pair<double, double>> square = {
        {0, 0}, {0, 10}, {10, 10}, {10, 0}
    };

    assert(point_in_polygon(0.0, 5.0, square) == true);
    assert(point_in_polygon(10.0, 5.0, square) == true);
    assert(point_in_polygon(0.0, 0.0, square) == true);
    assert(point_in_polygon(10.0, 10.0, square) == true);
    std::cout << "  PASS: boundary points\n";
}

void test_pip_degenerate() {
    // Less than 3 vertices
    const std::vector<std::pair<double, double>> line = {{0, 0}, {10, 10}};
    assert(point_in_polygon(5.0, 5.0, line) == false);

    const std::vector<std::pair<double, double>> empty = {};
    assert(point_in_polygon(0.0, 0.0, empty) == false);

    const std::vector<std::pair<double, double>> repeated_point = {
        {1, 1}, {1, 1}, {1, 1}, {1, 1}
    };
    assert(point_in_polygon(1.0, 1.0, repeated_point) == false);

    const std::vector<std::pair<double, double>> collinear = {
        {0, 0}, {5, 5}, {10, 10}
    };
    assert(point_in_polygon(5.0, 5.0, collinear) == false);
    std::cout << "  PASS: degenerate cases\n";
}

int main() {
    std::cout << "test_point_in_polygon:\n";
    test_pip_open_and_closed_rings();
    test_pip_vertex_order();
    test_pip_concave_polygon();
    test_pip_boundary_points_are_inside();
    test_pip_degenerate();
    std::cout << "All point-in-polygon tests passed.\n";
    return 0;
}
