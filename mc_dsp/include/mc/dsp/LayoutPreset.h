#pragma once

#include <optional>
#include <vector>

namespace mc::dsp {

struct SpeakerDirection {
    double azimuthDeg = 0.0;
    double elevationDeg = 0.0;
    bool lfe = false;
};

// Layout preset indices, matching the reference's own numbering
// (docs/ARCHITECTURE.md §0's layoutPreset row) so a saved `layoutIdx` means
// the same thing here as it did there.
enum class Layout {
    RingFrontLeft = 0,
    RingFrontCentre = 1,
    Pairs = 2,
    Standard = 3, // ITU: stereo/quad/5.1/7.1/7.1.4 depending on n
    TwoRings = 4,
    Dome = 5,
    Sphere = 6,
    Custom = 7, // no preset angles; caller supplies its own
};

// Speaker/track positions for a layout preset at n speakers/tracks, ported
// from the reference's `layoutPreset`. Returns std::nullopt for
// Layout::Custom (there is nothing to compute) and for any n the preset
// doesn't have a numbered-channel table for.
std::optional<std::vector<SpeakerDirection>> layoutPreset(Layout layout, int n);

} // namespace mc::dsp
