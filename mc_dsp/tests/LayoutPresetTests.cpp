#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "mc/dsp/LayoutPreset.h"

using namespace mc::dsp;

namespace {
void requireAz(const std::vector<SpeakerDirection>& dirs, std::size_t i, double expected) {
    REQUIRE(dirs[i].azimuthDeg == Catch::Approx(expected).margin(0.05));
}
} // namespace

TEST_CASE("Custom layout has no preset angles", "[dsp][layout]") {
    REQUIRE_FALSE(layoutPreset(Layout::Custom, 4).has_value());
}

TEST_CASE("Ring front-left quad matches the reference's documented angles", "[dsp][layout]") {
    // Reference: "4 tracks: 45°, −45°, −135°, 135°"
    auto dirs = layoutPreset(Layout::RingFrontLeft, 4);
    REQUIRE(dirs.has_value());
    requireAz(*dirs, 0, 45.0);
    requireAz(*dirs, 1, -45.0);
    requireAz(*dirs, 2, -135.0);
    requireAz(*dirs, 3, 135.0);
}

TEST_CASE("Ring front-left octophonic matches the reference's documented angles", "[dsp][layout]") {
    // Reference: "8 tracks: 22.5°, −22.5°, −67.5° …"
    auto dirs = layoutPreset(Layout::RingFrontLeft, 8);
    REQUIRE(dirs.has_value());
    requireAz(*dirs, 0, 22.5);
    requireAz(*dirs, 1, -22.5);
    requireAz(*dirs, 2, -67.5);
}

TEST_CASE("Pairs layout alternates left/right front to back", "[dsp][layout]") {
    // Reference: "8 tracks: ±22.5°, ±67.5°, ±112.5°, ±157.5°"
    auto dirs = layoutPreset(Layout::Pairs, 8);
    REQUIRE(dirs.has_value());
    requireAz(*dirs, 0, 22.5);
    requireAz(*dirs, 1, -22.5);
    requireAz(*dirs, 2, 67.5);
    requireAz(*dirs, 3, -67.5);
    requireAz(*dirs, 4, 112.5);
    requireAz(*dirs, 5, -112.5);
    requireAz(*dirs, 6, 157.5);
    requireAz(*dirs, 7, -157.5);
}

TEST_CASE("Pairs with an odd count sends the last track to the back", "[dsp][layout]") {
    auto dirs = layoutPreset(Layout::Pairs, 5);
    REQUIRE(dirs.has_value());
    // wrapAz folds exactly 180 degrees to -180 (the same direction, due
    // to the reference's own ((a + 180) % 360) - 180 wrap formula), so
    // "sent to the back" reads as -180 here, not +180.
    requireAz(*dirs, 4, -180.0);
}

TEST_CASE("Standard stereo is at +-30 degrees", "[dsp][layout]") {
    auto dirs = layoutPreset(Layout::Standard, 2);
    REQUIRE(dirs.has_value());
    requireAz(*dirs, 0, 30.0);
    requireAz(*dirs, 1, -30.0);
}

TEST_CASE("Standard quad is ITU (+-45, +-135)", "[dsp][layout]") {
    auto dirs = layoutPreset(Layout::Standard, 4);
    REQUIRE(dirs.has_value());
    requireAz(*dirs, 0, 45.0);
    requireAz(*dirs, 1, -45.0);
    requireAz(*dirs, 2, 135.0);
    requireAz(*dirs, 3, -135.0);
}

TEST_CASE("Standard 5.1 has one LFE at track index 3", "[dsp][layout]") {
    auto dirs = layoutPreset(Layout::Standard, 6);
    REQUIRE(dirs.has_value());
    int lfeCount = 0;
    for (std::size_t i = 0; i < dirs->size(); ++i) {
        if ((*dirs)[i].lfe) {
            ++lfeCount;
            REQUIRE(i == 3);
        }
    }
    REQUIRE(lfeCount == 1);
}

TEST_CASE("Standard 7.1 has 8 speakers, one LFE", "[dsp][layout]") {
    auto dirs = layoutPreset(Layout::Standard, 8);
    REQUIRE(dirs.has_value());
    REQUIRE(dirs->size() == 8);
    int lfeCount = 0;
    for (const auto& d : *dirs) {
        if (d.lfe) ++lfeCount;
    }
    REQUIRE(lfeCount == 1);
}

TEST_CASE("Standard 7.1.4 extends 7.1 with four elevated speakers", "[dsp][layout]") {
    auto dirs = layoutPreset(Layout::Standard, 12);
    REQUIRE(dirs.has_value());
    REQUIRE(dirs->size() == 12);
    int elevatedCount = 0;
    for (const auto& d : *dirs) {
        if (d.elevationDeg > 0.0) ++elevatedCount;
    }
    REQUIRE(elevatedCount == 4);
}

TEST_CASE("Standard falls back to Pairs for an unlisted channel count", "[dsp][layout]") {
    auto standardDirs = layoutPreset(Layout::Standard, 11);
    auto pairsDirs = layoutPreset(Layout::Pairs, 11);
    REQUIRE(standardDirs.has_value());
    REQUIRE(pairsDirs.has_value());
    for (std::size_t i = 0; i < standardDirs->size(); ++i) {
        REQUIRE((*standardDirs)[i].azimuthDeg == Catch::Approx((*pairsDirs)[i].azimuthDeg));
    }
}

TEST_CASE("Two rings splits speakers half at ear height, half elevated", "[dsp][layout]") {
    // Reference: "8 tracks = 4 + 4, 16 = 8 + 8"
    auto dirs = layoutPreset(Layout::TwoRings, 8);
    REQUIRE(dirs.has_value());
    REQUIRE(dirs->size() == 8);
    int flat = 0, elevated = 0;
    for (const auto& d : *dirs) {
        if (d.elevationDeg == 0.0) ++flat;
        if (d.elevationDeg == 35.0) ++elevated;
    }
    REQUIRE(flat == 4);
    REQUIRE(elevated == 4);
}

TEST_CASE("Dome puts exactly one speaker straight up", "[dsp][layout]") {
    // Reference: "13 = 8 + 4 + 1, 19 = 12 + 6 + 1"
    auto dirs = layoutPreset(Layout::Dome, 13);
    REQUIRE(dirs.has_value());
    REQUIRE(dirs->size() == 13);
    int top = 0;
    for (const auto& d : *dirs) {
        if (d.elevationDeg == 90.0) ++top;
    }
    REQUIRE(top == 1);
}

TEST_CASE("Sphere returns n evenly spread directions", "[dsp][layout]") {
    auto dirs = layoutPreset(Layout::Sphere, 50);
    REQUIRE(dirs.has_value());
    REQUIRE(dirs->size() == 50);
}

TEST_CASE("A single speaker/track is always straight ahead", "[dsp][layout]") {
    for (auto layout : {Layout::RingFrontLeft, Layout::Standard, Layout::Dome}) {
        auto dirs = layoutPreset(layout, 1);
        REQUIRE(dirs.has_value());
        REQUIRE(dirs->size() == 1);
        requireAz(*dirs, 0, 0.0);
    }
}
