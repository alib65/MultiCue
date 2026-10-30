#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

#include "mc/dsp/FumaConversion.h"

using namespace mc::dsp;

TEST_CASE("W (FuMa channel 0) maps to ACN 0 with gain sqrt(2)", "[dsp][fuma]") {
    auto [acn, gain] = fumaToAmbix(0);
    REQUIRE(acn == 0);
    REQUIRE(gain == Catch::Approx(std::sqrt(2.0)));
}

TEST_CASE("X, Y, Z (FuMa 1,2,3) map to ACN 3, 1, 2 with unity gain", "[dsp][fuma]") {
    REQUIRE(fumaToAmbix(1).first == 3);
    REQUIRE(fumaToAmbix(1).second == Catch::Approx(1.0));
    REQUIRE(fumaToAmbix(2).first == 1);
    REQUIRE(fumaToAmbix(3).first == 2);
}

TEST_CASE("Every first-order FuMa channel (0-3) maps to a distinct ACN 0-3", "[dsp][fuma]") {
    std::vector<int> acns;
    for (int i = 0; i < 4; ++i) {
        acns.push_back(fumaToAmbix(i).first);
    }
    std::sort(acns.begin(), acns.end());
    REQUIRE(acns == std::vector<int>{0, 1, 2, 3});
}

TEST_CASE("All 16 FuMa channels (up to 3rd order) map to distinct ACN indices with positive gain", "[dsp][fuma]") {
    std::vector<int> acns;
    for (int i = 0; i < kFumaMaxChannels; ++i) {
        auto [acn, gain] = fumaToAmbix(i);
        REQUIRE(acn >= 0);
        REQUIRE(acn < 16);
        REQUIRE(gain > 0.0);
        acns.push_back(acn);
    }
    std::sort(acns.begin(), acns.end());
    for (int i = 0; i < kFumaMaxChannels; ++i) {
        REQUIRE(acns[static_cast<std::size_t>(i)] == i);
    }
}

TEST_CASE("Out-of-range FuMa channel index is rejected, not a crash", "[dsp][fuma]") {
    REQUIRE(fumaToAmbix(-1).first == -1);
    REQUIRE(fumaToAmbix(16).first == -1);
    REQUIRE(fumaToAmbix(999).first == -1);
}
