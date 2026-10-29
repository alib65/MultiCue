#include <catch2/catch_test_macros.hpp>

#include "mc/core/Piece.h"

using namespace mc::core;

TEST_CASE("pieceDurationSeconds is 0 for a piece with no tracks", "[core][piece]") {
    Piece p;
    REQUIRE(pieceDurationSeconds(p) == 0.0);
}

TEST_CASE("pieceDurationSeconds is the longest track's duration", "[core][piece]") {
    Piece p;
    Track a;
    a.fileDurationSeconds = 3.0;
    Track b;
    b.fileDurationSeconds = 7.5;
    Track c;
    c.fileDurationSeconds = 2.0;
    p.tracks = {a, b, c};
    REQUIRE(pieceDurationSeconds(p) == 7.5);
}
