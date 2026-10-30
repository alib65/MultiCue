#include "mc/engine/SpatialLayout.h"

#include "mc/dsp/LayoutPreset.h"

namespace mc::engine {

void applyLayoutToPiece(mc::core::Piece& piece, int layoutIdx) {
    if (layoutIdx < 0 || layoutIdx > 6) {
        piece.layoutIdx = -1; // Custom: leave existing per-track angles alone
        return;
    }
    auto dirs = mc::dsp::layoutPreset(static_cast<mc::dsp::Layout>(layoutIdx), static_cast<int>(piece.tracks.size()));
    if (!dirs.has_value()) {
        return;
    }
    for (std::size_t i = 0; i < piece.tracks.size() && i < dirs->size(); ++i) {
        piece.tracks[i].azimuthDeg = (*dirs)[i].azimuthDeg;
        piece.tracks[i].elevationDeg = (*dirs)[i].elevationDeg;
        piece.tracks[i].lfe = (*dirs)[i].lfe;
    }
    piece.layoutIdx = layoutIdx;
}

VenueLayout buildVenueLayoutFromPreset(int layoutIdx, std::size_t numOutputs) {
    VenueLayout venue;
    venue.numOutputs = numOutputs;
    if (layoutIdx < 0 || layoutIdx > 6) {
        layoutIdx = 0;
    }
    auto dirs = mc::dsp::layoutPreset(static_cast<mc::dsp::Layout>(layoutIdx), static_cast<int>(numOutputs));
    if (!dirs.has_value()) {
        return venue;
    }
    for (std::size_t i = 0; i < dirs->size(); ++i) {
        if ((*dirs)[i].lfe) {
            venue.subOutputIndices.push_back(i);
        } else {
            venue.realOutputIndices.push_back(i);
            venue.realDirections.emplace_back((*dirs)[i].azimuthDeg, (*dirs)[i].elevationDeg);
        }
    }
    return venue;
}

} // namespace mc::engine
