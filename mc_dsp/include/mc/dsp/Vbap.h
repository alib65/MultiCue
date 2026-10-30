#pragma once

#include <array>
#include <utility>
#include <vector>

namespace mc::dsp {

// Vector-base amplitude panning (Pulkki 1997), ported from the reference's
// `vbapSetup`/`vbapGains` (docs/ARCHITECTURE.md §0): a convex-hull
// triangulation of the real speakers plus "imaginary" speakers at the 26
// cube corner/edge/face directions wherever they are more than 70 degrees
// from every real speaker, filling gaps in the layout (e.g. above a ring,
// or behind a stereo pair). Imaginary-speaker gains are discarded and the
// result renormalised to unit power.
//
// vbapSetup() is the non-real-time setup step (does the triangulation);
// vbapGains() is cheap enough to call per block/per source.
struct VbapSetup {
    std::vector<std::array<double, 3>> points; // real speakers first, then imaginary ones
    std::size_t numReal = 0;
    // Each facet: three point indices plus the inverse-matrix rows used to
    // solve for barycentric-style gains (see Pulkki's paper).
    struct Facet {
        std::array<std::size_t, 3> indices;
        std::array<std::array<double, 3>, 3> inverseRows;
    };
    std::vector<Facet> facets;
};

VbapSetup vbapSetup(const std::vector<std::pair<double, double>>& realSpeakerDirsAzEl);

// Gains for the real speakers only (imaginary speakers are solved for
// internally but never returned), for a source at (azimuthDeg, elevationDeg).
// With renorm=true (the default, used for live panning) the result is
// rescaled to unit power even when some gain went to an imaginary speaker,
// so energy stays constant as a source moves through a gap in the layout.
// renorm=false is used internally by the Ambisonic decoders, which do
// their own normalisation pass afterward.
std::vector<double> vbapGains(const VbapSetup& setup, double azimuthDeg, double elevationDeg, bool renorm = true);

} // namespace mc::dsp
