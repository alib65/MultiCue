#pragma once

#include <vector>

#include "mc/core/Piece.h"

namespace mc::core {

struct PlaylistSettings {
    bool autoAdvance = true;
    double gapSeconds = 3.0;
};

struct Playlist {
    PlaylistSettings settings;
    std::vector<Piece> pieces;
};

} // namespace mc::core
