#pragma once

#include <string>
#include <vector>

#include "mc/core/Track.h"

namespace mc::core {

// A piece is one or more mono tracks played together, sample-locked
// (docs/ARCHITECTURE.md §3.6). Source format and spatial layout (AmbiX,
// FuMa, az/el per track) are added at M3 alongside the spatial math that
// gives them meaning; M2 is direct mode only, so a Piece is just tracks and
// a trim.
struct Piece {
    std::string title;
    float trimDb = 0.0f;
    std::vector<Track> tracks;
};

// Longest track in the piece, matching the reference's pieceSecs (the piece
// plays for as long as its longest file).
double pieceDurationSeconds(const Piece& piece);

} // namespace mc::core
