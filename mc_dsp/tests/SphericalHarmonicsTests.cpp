#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "mc/dsp/SphericalHarmonics.h"

using namespace mc::dsp;

TEST_CASE("ACN0 (W) is always 1 regardless of direction", "[dsp][sh]") {
    for (double az : {0.0, 45.0, 90.0, 180.0, -60.0}) {
        for (double el : {-90.0, -30.0, 0.0, 30.0, 90.0}) {
            auto y = sphericalHarmonicsSN3D(3, az, el);
            REQUIRE(y[0] == Catch::Approx(1.0).margin(1e-9));
        }
    }
}

TEST_CASE("Returns (order+1)^2 coefficients", "[dsp][sh]") {
    for (int order = 0; order <= 7; ++order) {
        auto y = sphericalHarmonicsSN3D(order, 10.0, 5.0);
        REQUIRE(y.size() == static_cast<std::size_t>((order + 1) * (order + 1)));
    }
}

TEST_CASE("Zonal harmonics (m=0) do not depend on azimuth", "[dsp][sh]") {
    for (int order = 1; order <= 5; ++order) {
        const auto y1 = sphericalHarmonicsSN3D(order, 0.0, 25.0);
        const auto y2 = sphericalHarmonicsSN3D(order, 137.0, 25.0);
        for (int n = 0; n <= order; ++n) {
            const auto idx = static_cast<std::size_t>((n * n) + n);
            REQUIRE(y1[idx] == Catch::Approx(y2[idx]).margin(1e-9));
        }
    }
}

TEST_CASE("No NaN/Inf across a grid of directions up to 7th order", "[dsp][sh]") {
    for (int order = 0; order <= 7; ++order) {
        for (int azI = -180; azI <= 180; azI += 15) {
            for (int elI = -90; elI <= 90; elI += 15) {
                auto y = sphericalHarmonicsSN3D(order, azI, elI);
                for (double v : y) {
                    REQUIRE(std::isfinite(v));
                }
            }
        }
    }
}

TEST_CASE("maxReWeights returns order+1 finite weights, first is 1", "[dsp][sh]") {
    for (int order = 0; order <= 7; ++order) {
        auto w = maxReWeights(order);
        REQUIRE(w.size() == static_cast<std::size_t>(order + 1));
        REQUIRE(w[0] == Catch::Approx(1.0).margin(1e-9)); // P_0(x) == 1 always
        for (double v : w) {
            REQUIRE(std::isfinite(v));
        }
    }
}

TEST_CASE("legendreP matches well-known closed forms", "[dsp][sh]") {
    REQUIRE(legendreP(0, 0.3) == Catch::Approx(1.0));
    REQUIRE(legendreP(1, 0.3) == Catch::Approx(0.3));
    // P_2(x) = (3x^2 - 1) / 2
    REQUIRE(legendreP(2, 0.3) == Catch::Approx((3 * 0.3 * 0.3 - 1) / 2.0).margin(1e-9));
    // P_3(x) = (5x^3 - 3x) / 2
    REQUIRE(legendreP(3, 0.5) == Catch::Approx((5 * 0.125 - 3 * 0.5) / 2.0).margin(1e-9));
}

TEST_CASE("Energy integrates to a direction-independent constant (Parseval check)", "[dsp][sh]") {
    // sum_i Y_i(dir)^2 should be the same for every direction, for a fixed
    // order (this is the defining property of an orthonormal-on-average
    // SH basis evaluated pointwise -- a strong cross-check that the port's
    // normalisation is self-consistent, independent of any particular
    // axis/sign convention).
    for (int order = 1; order <= 5; ++order) {
        auto energyAt = [&](double az, double el) {
            auto y = sphericalHarmonicsSN3D(order, az, el);
            double e = 0.0;
            for (double v : y) e += v * v;
            return e;
        };
        const double e0 = energyAt(0.0, 0.0);
        REQUIRE(energyAt(73.0, 12.0) == Catch::Approx(e0).margin(1e-6));
        REQUIRE(energyAt(-140.0, -40.0) == Catch::Approx(e0).margin(1e-6));
        REQUIRE(energyAt(170.0, 80.0) == Catch::Approx(e0).margin(1e-6));
    }
}
