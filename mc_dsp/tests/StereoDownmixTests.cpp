#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "mc/dsp/StereoDownmix.h"

using namespace mc::dsp;

TEST_CASE("Center front is equal power left/right", "[dsp][stereo]") {
    auto g = stereoDownmixGains(0.0, 0.0);
    REQUIRE(g[0] == Catch::Approx(g[1]).margin(1e-6));
    REQUIRE(g[0] == Catch::Approx(std::sqrt(0.5)).margin(1e-6));
}

TEST_CASE("A source at +30 degrees or wider goes fully left", "[dsp][stereo]") {
    auto g30 = stereoDownmixGains(30.0, 0.0);
    REQUIRE(g30[0] == Catch::Approx(1.0).margin(1e-6));
    REQUIRE(g30[1] == Catch::Approx(0.0).margin(1e-6));

    auto g60 = stereoDownmixGains(60.0, 0.0);
    REQUIRE(g60[0] == Catch::Approx(1.0).margin(1e-6));
}

TEST_CASE("A source at -30 degrees or wider goes fully right", "[dsp][stereo]") {
    auto g = stereoDownmixGains(-30.0, 0.0);
    REQUIRE(g[1] == Catch::Approx(1.0).margin(1e-6));
    REQUIRE(g[0] == Catch::Approx(0.0).margin(1e-6));
}

TEST_CASE("Constant power: gain-squared sums to 1 in front, less than 1 in the rear", "[dsp][stereo]") {
    auto front = stereoDownmixGains(10.0, 0.0);
    REQUIRE((front[0] * front[0]) + (front[1] * front[1]) == Catch::Approx(1.0).margin(1e-6));

    auto rear = stereoDownmixGains(170.0, 0.0);
    const double rearPower = (rear[0] * rear[0]) + (rear[1] * rear[1]);
    REQUIRE(rearPower < 1.0);
}

TEST_CASE("A source directly behind folds symmetrically to the front", "[dsp][stereo]") {
    auto g = stereoDownmixGains(180.0, 0.0);
    REQUIRE(g[0] == Catch::Approx(g[1]).margin(1e-6));
}

TEST_CASE("Rear-left and rear-right are mirror images", "[dsp][stereo]") {
    auto left = stereoDownmixGains(150.0, 0.0);
    auto right = stereoDownmixGains(-150.0, 0.0);
    REQUIRE(left[0] == Catch::Approx(right[1]).margin(1e-6));
    REQUIRE(left[1] == Catch::Approx(right[0]).margin(1e-6));
}
