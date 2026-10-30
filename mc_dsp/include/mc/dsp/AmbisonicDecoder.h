#pragma once

#include <utility>
#include <vector>

namespace mc::dsp {

// All three decoders below return a [numOutputRows][numAmbisonicChannels]
// matrix: row r, column acn is the gain applied to Ambisonic channel `acn`
// to produce output row r. Channels are ACN order, SN3D normalisation.

// True if every direction is within +-10 degrees of ear height -- the
// reference's own threshold for choosing the (cheaper, 2D-specific)
// decoder over AllRAD.
bool isLayout2D(const std::vector<std::pair<double, double>>& dirsAzEl);

// 2D max-rE decoder for horizontal layouts (rings): samples a virtual ring
// of 72 points, pairwise-pans each to the real speakers via VBAP, and
// accumulates weighted Ambisonic basis rows -- ported from the reference's
// `decoder2D`. Energy-normalised against a 72-point test ring.
std::vector<std::vector<double>> decode2D(const std::vector<std::pair<double, double>>& speakerDirsAzEl, int order);

// AllRAD (Zotter & Frank): max-rE sampling decoder to 240 Fibonacci-sphere
// virtual speakers, VBAP-panned to the real speakers; falls back to
// decode2D() for horizontal layouts, same as the reference's `allradDecoder`.
// Energy-normalised against a 200-point test grid.
std::vector<std::vector<double>> allRadDecoder(const std::vector<std::pair<double, double>>& speakerDirsAzEl, int order);

// Decodes directly to a small virtual-speaker array (no VBAP involved) --
// used for binaural/stereo rendering of Ambisonic *pieces*, ported from the
// reference's `virtualDecoder`.
std::vector<std::vector<double>> virtualSpeakerDecoder(const std::vector<std::pair<double, double>>& virtualDirsAzEl, int order);

} // namespace mc::dsp
