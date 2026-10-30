#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "mc/dsp/FibonacciSphere.h"

using namespace mc::dsp;

TEST_CASE("fibonacciSphere returns exactly n points", "[dsp][fibsphere]") {
    for (int n : {1, 8, 50, 240}) {
        REQUIRE(fibonacciSphere(n).size() == static_cast<std::size_t>(n));
    }
}

TEST_CASE("fibonacciSphere returns valid az/el ranges", "[dsp][fibsphere]") {
    const auto pts = fibonacciSphere(100);
    for (const auto& [az, el] : pts) {
        REQUIRE(az >= -180.0);
        REQUIRE(az <= 180.0);
        REQUIRE(el >= -90.0);
        REQUIRE(el <= 90.0);
    }
}

TEST_CASE("fibonacciSphere points lie on the unit sphere", "[dsp][fibsphere]") {
    constexpr double kDeg = 3.14159265358979323846 / 180.0;
    const auto pts = fibonacciSphere(50);
    for (const auto& [az, el] : pts) {
        const double x = std::cos(az * kDeg) * std::cos(el * kDeg);
        const double y = std::sin(az * kDeg) * std::cos(el * kDeg);
        const double z = std::sin(el * kDeg);
        REQUIRE((x * x + y * y + z * z) == Catch::Approx(1.0).margin(1e-9));
    }
}

TEST_CASE("fibonacciSphere of 0 or negative is empty, not a crash", "[dsp][fibsphere]") {
    REQUIRE(fibonacciSphere(0).empty());
    REQUIRE(fibonacciSphere(-5).empty());
}
