#pragma once

#include <cstddef>
#include <vector>

namespace mc::dsp {

// Real spherical harmonics in ACN channel order with SN3D normalisation
// (the AmbiX convention), no Condon-Shortley phase -- ported verbatim from
// the reference's `shSN3D` (docs/ARCHITECTURE.md §0). Directions use the
// project's convention throughout mc_dsp: azimuth in degrees, 0 = front,
// +90 = left, -90 = right, 180 = back; elevation in degrees, 0 = ear
// height, +90 = straight up.
//
// Returns (order+1)^2 coefficients, index = n^2 + n + m for channel (n, m).
std::vector<double> sphericalHarmonicsSN3D(int order, double azimuthDeg, double elevationDeg);

// max-rE weighting per spherical-harmonic degree n (0..order), used by the
// AllRAD/2D Ambisonic decoders to control the decoder's directivity --
// ported from the reference's `maxReWeights`.
std::vector<double> maxReWeights(int order);

// Legendre polynomial P_n(x), used internally by maxReWeights (exposed
// because it is independently testable against well-known closed forms).
double legendreP(int n, double x);

} // namespace mc::dsp
