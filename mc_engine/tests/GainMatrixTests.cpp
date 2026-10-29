#include <catch2/catch_test_macros.hpp>

#include "mc/engine/GainMatrix.h"

using mc::engine::GainMatrix;

TEST_CASE("identityDirect maps track i to output firstOutput+i at unity gain", "[engine][gainmatrix]") {
    auto m = GainMatrix::identityDirect(3, 4, 1);
    REQUIRE(m.gain(0, 1) == 1.0f);
    REQUIRE(m.gain(1, 2) == 1.0f);
    REQUIRE(m.gain(2, 3) == 1.0f);
    REQUIRE(m.gain(0, 0) == 0.0f);
    REQUIRE(m.gain(0, 2) == 0.0f);
}

TEST_CASE("identityDirect drops tracks beyond the available outputs", "[engine][gainmatrix]") {
    auto m = GainMatrix::identityDirect(4, 2, 0);
    REQUIRE(m.gain(0, 0) == 1.0f);
    REQUIRE(m.gain(1, 1) == 1.0f);
    // tracks 2 and 3 have nowhere to go; every output gain for them is 0.
    REQUIRE(m.gain(2, 0) == 0.0f);
    REQUIRE(m.gain(2, 1) == 0.0f);
    REQUIRE(m.gain(3, 0) == 0.0f);
    REQUIRE(m.gain(3, 1) == 0.0f);
}

TEST_CASE("out-of-range source/output reads return 0 rather than crashing", "[engine][gainmatrix]") {
    GainMatrix m(2, 2);
    REQUIRE(m.gain(5, 0) == 0.0f);
    REQUIRE(m.gain(0, 5) == 0.0f);
}

TEST_CASE("setGain is readable back", "[engine][gainmatrix]") {
    GainMatrix m(2, 2);
    m.setGain(0, 1, 0.5f);
    REQUIRE(m.gain(0, 1) == 0.5f);
    REQUIRE(m.gain(1, 0) == 0.0f);
}
