#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "mc/dsp/SphericalHeadHrtf.h"

using namespace mc::dsp;

TEST_CASE("A source dead ahead has equal ITD for both ears", "[dsp][hrtf]") {
    auto h = sphericalHeadHrtfCoeffs(0.0, 0.0, 48000.0);
    REQUIRE(h.left.delaySeconds == Catch::Approx(h.right.delaySeconds).margin(1e-9));
}

TEST_CASE("A source to the left arrives at the left ear first (shorter delay)", "[dsp][hrtf]") {
    auto h = sphericalHeadHrtfCoeffs(90.0, 0.0, 48000.0); // +90 = left
    REQUIRE(h.left.delaySeconds < h.right.delaySeconds);
}

TEST_CASE("A source to the right arrives at the right ear first", "[dsp][hrtf]") {
    auto h = sphericalHeadHrtfCoeffs(-90.0, 0.0, 48000.0); // -90 = right
    REQUIRE(h.right.delaySeconds < h.left.delaySeconds);
}

TEST_CASE("Left/right are mirror images of each other across the midline", "[dsp][hrtf]") {
    auto left = sphericalHeadHrtfCoeffs(60.0, 0.0, 48000.0);
    auto right = sphericalHeadHrtfCoeffs(-60.0, 0.0, 48000.0);
    REQUIRE(left.left.delaySeconds == Catch::Approx(right.right.delaySeconds).margin(1e-9));
    REQUIRE(left.right.delaySeconds == Catch::Approx(right.left.delaySeconds).margin(1e-9));
}

TEST_CASE("ITD never exceeds the time for sound to travel head-circumference-ish distances", "[dsp][hrtf]") {
    // A generous sanity bound: the spherical head model's max possible path
    // difference is on the order of (pi * radius) / c, well under 1 ms for
    // a human-sized head.
    for (double az = -180.0; az < 180.0; az += 10.0) {
        auto h = sphericalHeadHrtfCoeffs(az, 0.0, 48000.0);
        REQUIRE(h.left.delaySeconds >= 0.0);
        REQUIRE(h.right.delaySeconds >= 0.0);
        REQUIRE(h.left.delaySeconds < 0.001);
        REQUIRE(h.right.delaySeconds < 0.001);
    }
}

TEST_CASE("All filter coefficients are finite across the full sphere", "[dsp][hrtf]") {
    for (double az = -180.0; az < 180.0; az += 15.0) {
        for (double el = -90.0; el <= 90.0; el += 15.0) {
            auto h = sphericalHeadHrtfCoeffs(az, el, 48000.0);
            REQUIRE(std::isfinite(h.left.a));
            REQUIRE(std::isfinite(h.left.b));
            REQUIRE(std::isfinite(h.left.c));
            REQUIRE(std::isfinite(h.right.a));
            REQUIRE(std::isfinite(h.shelfA));
            REQUIRE(std::isfinite(h.shelfB));
            REQUIRE(std::isfinite(h.shelfC));
        }
    }
}

TEST_CASE("A very low sample rate is clamped, not divided-by-zero", "[dsp][hrtf]") {
    auto h = sphericalHeadHrtfCoeffs(0.0, 0.0, 100.0);
    REQUIRE(std::isfinite(h.left.a));
}
