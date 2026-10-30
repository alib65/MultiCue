#pragma once

#include <cstddef>

#include "mc/core/Piece.h"
#include "mc/engine/SpatialRenderer.h"

namespace mc::engine {

// Fills every track's azimuthDeg/elevationDeg/lfe from a layout preset
// (mc::dsp::Layout numbering), and sets layoutIdx accordingly -- ported
// from the reference's `applyLayoutToPiece`. A no-op for pieces whose
// track count the preset has no table for (layoutPreset() returning
// nullopt) and for layoutIdx == -1 (Custom: the piece keeps whatever
// per-track angles it already has). Only meaningful for
// SourceFormat::Multi pieces.
void applyLayoutToPiece(mc::core::Piece& piece, int layoutIdx);

// Builds a VenueLayout (SpatialRenderer.h) from a layout preset sized to
// `numOutputs` -- the substitute for a "My speakers" editor
// (docs/ROADMAP.md M3 open item: that editor, with per-output custom
// angles and distance compensation, doesn't exist yet).
VenueLayout buildVenueLayoutFromPreset(int layoutIdx, std::size_t numOutputs);

} // namespace mc::engine
