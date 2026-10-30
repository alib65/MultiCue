#pragma once

#include <array>

namespace mc::dsp {

// Constant-power stereo downmix gains {gainL, gainR} for a source at
// (azimuthDeg, elevationDeg): sources at +-30 degrees or wider go fully to
// one side, sources behind the listener are folded to the front and
// slightly attenuated -- ported verbatim from the reference's
// `stereoGains` (docs/ARCHITECTURE.md §0).
std::array<double, 2> stereoDownmixGains(double azimuthDeg, double elevationDeg);

} // namespace mc::dsp
