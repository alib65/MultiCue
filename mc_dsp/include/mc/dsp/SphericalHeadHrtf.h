#pragma once

namespace mc::dsp {

// First-order shelf/shadow filter coefficients for one ear: a bilinear-
// transformed one-pole filter, y[n] = a*x[n] + b*x[n-1] + c*y[n-1]
// (the reference's `FOS.ar(sig, a, b, c)` convention), plus the interaural
// delay for this ear in seconds.
struct EarFilterCoeffs {
    double delaySeconds = 0.0;
    double a = 1.0;
    double b = 0.0;
    double c = 0.0;
};

// Fallback (non-SOFA) binaural model: a spherical head (Brown & Duda 1998)
// -- interaural time difference via a path-length-around-the-head model,
// a head-shadow filter per ear, and a shared front/back high-frequency
// shelf -- ported from the reference's `binCoefs`. SOFA/measured HRTFs are
// the primary path (project brief §5); this is the fallback used when none
// is loaded.
struct SphericalHeadHrtf {
    EarFilterCoeffs left;
    EarFilterCoeffs right;
    // Shared front/back shelf, same FOS convention as EarFilterCoeffs.
    double shelfA = 1.0;
    double shelfB = 0.0;
    double shelfC = 0.0;
};

SphericalHeadHrtf sphericalHeadHrtfCoeffs(double azimuthDeg, double elevationDeg, double sampleRate);

} // namespace mc::dsp
