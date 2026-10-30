#include <catch2/catch_test_macros.hpp>

#include "mc/video/VideoDecoder.h"

using mc::video::selectPresentationFrameIndex;

TEST_CASE("selectPresentationFrameIndex picks the newest frame that is due", "[video]") {
    const std::vector<double> pts = {0.0, 0.5, 1.0, 1.5};
    REQUIRE(selectPresentationFrameIndex(pts, 1.2) == 2);
}

TEST_CASE("selectPresentationFrameIndex drops every earlier stale frame at once", "[video]") {
    const std::vector<double> pts = {0.0, 0.5, 1.0, 1.5, 2.0};
    // Caller fell behind; everything up through index 3 is now due.
    REQUIRE(selectPresentationFrameIndex(pts, 1.8) == 3);
}

TEST_CASE("selectPresentationFrameIndex holds the current frame when nothing is due yet", "[video]") {
    const std::vector<double> pts = {1.0, 2.0, 3.0};
    REQUIRE(selectPresentationFrameIndex(pts, 0.5) == -1);
}

TEST_CASE("selectPresentationFrameIndex on an empty queue holds the current frame", "[video]") {
    REQUIRE(selectPresentationFrameIndex({}, 5.0) == -1);
}

TEST_CASE("selectPresentationFrameIndex at an exact PTS match presents that frame", "[video]") {
    const std::vector<double> pts = {0.0, 1.0, 2.0};
    REQUIRE(selectPresentationFrameIndex(pts, 1.0) == 1);
}
