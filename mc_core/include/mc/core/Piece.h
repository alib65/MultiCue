#pragma once

#include <string>
#include <vector>

#include "mc/core/Track.h"

namespace mc::core {

// What the piece's files actually are, per docs/ARCHITECTURE.md §4.1.
// Added in schema v2 alongside listening-format rendering.
enum class SourceFormat {
    Multi, // speaker feeds, placed via each track's azimuthDeg/elevationDeg
    AmbiX, // ACN/SN3D, track i is Ambisonic channel i
    Fuma,  // Furse-Malham order/weighting, converted to AmbiX internally
};

// A piece is one or more mono tracks played together, sample-locked
// (docs/ARCHITECTURE.md §3.6).
struct Piece {
    std::string title;
    float trimDb = 0.0f;
    std::vector<Track> tracks;

    SourceFormat srcFormat = SourceFormat::Multi;
    // Layout preset index for Multi pieces, matching mc::dsp::Layout's
    // numbering (-1 = custom angles, not a preset). Irrelevant for AmbiX/
    // FuMa pieces.
    int layoutIdx = 0;

    // Optional video shown in sync with this piece's audio (mc_video,
    // M6 slice: docs/ARCHITECTURE.md Sec 3.6/7). Empty = no attached video;
    // the video's own embedded audio (if any) is never decoded -- the
    // tracks above are the piece's audio.
    std::string videoPath;
};

// Longest track in the piece, matching the reference's pieceSecs (the piece
// plays for as long as its longest file).
double pieceDurationSeconds(const Piece& piece);

} // namespace mc::core
