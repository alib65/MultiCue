#pragma once

#include <utility>
#include <vector>

namespace mc::dsp {

// n nearly-uniformly distributed directions on the unit sphere (Fibonacci
// spiral), ported from the reference's `fibSphere`. Used as a virtual-
// speaker grid for AllRAD and as a sampling grid for energy-normalisation
// checks. Returns {azimuthDeg, elevationDeg} pairs.
std::vector<std::pair<double, double>> fibonacciSphere(int n);

} // namespace mc::dsp
