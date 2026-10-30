#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "mc/dsp/Vbap.h"

using namespace mc::dsp;

namespace {
double sumSquares(const std::vector<double>& v) {
    double s = 0.0;
    for (double x : v) s += x * x;
    return s;
}
} // namespace

TEST_CASE("A direction exactly at a real speaker gets unity gain there, zero elsewhere", "[dsp][vbap]") {
    std::vector<std::pair<double, double>> quad = {{45, 0}, {-45, 0}, {-135, 0}, {135, 0}};
    auto setup = vbapSetup(quad);
    auto g = vbapGains(setup, 45.0, 0.0);
    REQUIRE(g.size() == 4);
    REQUIRE(g[0] == Catch::Approx(1.0).margin(1e-3));
    for (std::size_t i = 1; i < g.size(); ++i) {
        REQUIRE(g[i] == Catch::Approx(0.0).margin(1e-3));
    }
}

TEST_CASE("Renormalised gains always sum of squares to 1 (unit power)", "[dsp][vbap]") {
    std::vector<std::pair<double, double>> quad = {{45, 0}, {-45, 0}, {-135, 0}, {135, 0}};
    auto setup = vbapSetup(quad);
    for (double az : {0.0, 20.0, 45.0, 90.0, 135.0, 180.0, -60.0}) {
        for (double el : {0.0, 30.0, 60.0, 90.0}) {
            auto g = vbapGains(setup, az, el);
            REQUIRE(sumSquares(g) == Catch::Approx(1.0).margin(1e-3));
        }
    }
}

TEST_CASE("A direction between two adjacent real speakers pans only between them", "[dsp][vbap]") {
    std::vector<std::pair<double, double>> quad = {{45, 0}, {-45, 0}, {-135, 0}, {135, 0}};
    auto setup = vbapSetup(quad);
    auto g = vbapGains(setup, 0.0, 0.0); // exactly between speaker 0 (45) and speaker 1 (-45)
    REQUIRE(g[0] == Catch::Approx(g[1]).margin(1e-3));
    REQUIRE(g[2] == Catch::Approx(0.0).margin(1e-3));
    REQUIRE(g[3] == Catch::Approx(0.0).margin(1e-3));
    REQUIRE(sumSquares(g) == Catch::Approx(1.0).margin(1e-3));
}

TEST_CASE("Without renormalisation, a direction in a gap leaks energy to imaginary speakers", "[dsp][vbap]") {
    // A flat ring has no speaker overhead, so straight up is a "hole" --
    // some/all of the raw (pre-renorm) gain goes to an imaginary speaker.
    std::vector<std::pair<double, double>> quad = {{45, 0}, {-45, 0}, {-135, 0}, {135, 0}};
    auto setup = vbapSetup(quad);
    auto gRaw = vbapGains(setup, 0.0, 90.0, false);
    REQUIRE(sumSquares(gRaw) < 0.999);
}

TEST_CASE("A single real speaker always gets full gain", "[dsp][vbap]") {
    std::vector<std::pair<double, double>> one = {{0, 0}};
    auto setup = vbapSetup(one);
    auto g = vbapGains(setup, 90.0, 45.0);
    REQUIRE(g.size() == 1);
    REQUIRE(g[0] == Catch::Approx(1.0));
}

TEST_CASE("Gains are never negative", "[dsp][vbap]") {
    std::vector<std::pair<double, double>> octo = {{22.5, 0}, {-22.5, 0}, {-67.5, 0}, {67.5, 0}, {112.5, 0}, {-112.5, 0}, {157.5, 0}, {-157.5, 0}};
    auto setup = vbapSetup(octo);
    for (double az = -180; az < 180; az += 13) {
        for (double el = -90; el <= 90; el += 17) {
            auto g = vbapGains(setup, az, el);
            for (double v : g) {
                REQUIRE(v >= -1e-9);
            }
        }
    }
}
