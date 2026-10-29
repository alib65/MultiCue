#include "mc/core/Piece.h"

#include <algorithm>

namespace mc::core {

double pieceDurationSeconds(const Piece& piece) {
    double maxSeconds = 0.0;
    for (const auto& t : piece.tracks) {
        maxSeconds = std::max(maxSeconds, t.fileDurationSeconds);
    }
    return maxSeconds;
}

} // namespace mc::core
