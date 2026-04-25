/**
 * SignalRoute — Unit Tests: Point-in-Polygon
 *
 * TODO: Integrate with Google Test or Catch2
 */

#include "geofence/point_in_polygon.h"
#include <cassert>
#include <iostream>

void test_pip_simple_square() {
    // Square: (0,0), (0,10), (10,10), (10,0)
    std::vector<std::pair<double, double>> square = {
        {0, 0}, {0, 10}, {10, 10}, {10, 0}
    };

    assert(signalroute::geo::point_in_polygon(5.0, 5.0, square) == true);
    assert(signalroute::geo::point_in_polygon(15.0, 5.0, square) == false);
    assert(signalroute::geo::point_in_polygon(-1.0, 5.0, square) == false);
    std::cout << "  PASS: simple square\n";
}

void test_pip_triangle() {
    std::vector<std::pair<double, double>> triangle = {
        {0, 0}, {10, 5}, {0, 10}
    };

    assert(signalroute::geo::point_in_polygon(3.0, 5.0, triangle) == true);
    assert(signalroute::geo::point_in_polygon(8.0, 5.0, triangle) == true);
    assert(signalroute::geo::point_in_polygon(11.0, 5.0, triangle) == false);
    std::cout << "  PASS: triangle\n";
}

void test_pip_degenerate() {
    // Less than 3 vertices
    std::vector<std::pair<double, double>> line = {{0, 0}, {10, 10}};
    assert(signalroute::geo::point_in_polygon(5.0, 5.0, line) == false);

    std::vector<std::pair<double, double>> empty = {};
    assert(signalroute::geo::point_in_polygon(0.0, 0.0, empty) == false);
    std::cout << "  PASS: degenerate cases\n";
}

int main() {
    std::cout << "test_point_in_polygon:\n";
    test_pip_simple_square();
    test_pip_triangle();
    test_pip_degenerate();
    std::cout << "All point-in-polygon tests passed.\n";
    return 0;
}
