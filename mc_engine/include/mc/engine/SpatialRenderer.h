#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "mc/core/Piece.h"
#include "mc/engine/GainMatrix.h"

namespace mc::engine {

// The six listening formats from the project brief §4.2 / reference
// README (docs/ARCHITECTURE.md §0). Direct mode does not need this
// renderer at all (RealtimeAudioEngine builds its gain matrix straight
// from each track's outputChannel); this covers the other five.
enum class ListeningFormat {
    Ambisonics, // 3D Ambisonics -> my speakers (AllRAD / 2D max-rE)
    Vbap,       // amplitude panning -> my speakers
    Binaural,   // -> headphones (spherical-head HRTF)
    Stereo,     // constant-power downmix -> 2 speakers
    BFormat,    // Ambisonic B-format out (AmbiX)
};

// Where the venue's real (non-subwoofer) outputs are, and which device
// output indices are real vs. subwoofer. Until a "My speakers" editor
// exists (docs/ROADMAP.md M3 open item), the caller builds this from a
// layout preset sized to the output count.
struct VenueLayout {
    std::vector<std::pair<double, double>> realDirections; // az/el, parallel to realOutputIndices
    std::vector<std::size_t> realOutputIndices;
    std::vector<std::size_t> subOutputIndices;
    std::size_t numOutputs = 0;
};

struct SpatialRenderResult {
    // Non-binaural formats (Ambisonics/VBAP/Stereo/BFormat) produce a plain
    // [tracks x outputs] gain matrix, usable directly by PieceMixer. For
    // Binaural, `matrix` carries only LFE tracks' direct route to the
    // stereo output pair at a fixed gain (matching the reference: LFE
    // tracks bypass the HRTF renderer entirely, going "equally to both
    // ears" via the plain mix path, not through a virtual source).
    bool isBinaural = false;
    GainMatrix matrix;

    // Binaural's non-LFE tracks instead produce an intermediate
    // [tracks x virtualSources] matrix: each virtual source is a mono
    // signal (a track itself for Multi-format pieces, a weighted sum of
    // Ambisonic channels for AmbiX/FuMa pieces) that still needs per-
    // direction HRTF filtering, which is real-time DSP state a binaural
    // rendering stage owns, not something a stateless renderer can hand
    // back as a matrix.
    GainMatrix sourceToVirtual;
    std::vector<std::pair<double, double>> virtualDirections;

    int effectiveOrder = 0;
    double stereoStartChannelUsed = 0; // echoes back the start channel actually used
    std::string note;                  // short status line, like the reference's blue info line
};

// `stereoStartChannel` is the first of two output channels Stereo/Binaural
// write to -- configurable here, fixing the reference's "always starts at
// output 1" limitation (project brief §5).
SpatialRenderResult renderListeningFormat(ListeningFormat format,
                                          int orderOverride, // 0 = Auto
                                          const mc::core::Piece& piece,
                                          const VenueLayout& venue,
                                          int stereoStartChannel = 0);

} // namespace mc::engine
