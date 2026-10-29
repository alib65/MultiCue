#pragma once

#include <cstddef>

namespace mc::engine {

// Writes nFrames of a sine tone into an interleaved output buffer, on the
// single channel `targetChannel` (0-based) out of `numChannels`, silence on
// every other channel. Pure, allocation-free and exception-free: this is the
// function the real-time audio callback calls directly, so it must honor the
// real-time rules in docs/ARCHITECTURE.md §3.1 (no allocation, no locks, no
// I/O) to be safe to call from the audio thread.
//
// `phase` is in/out: radians, wrapped to [0, 2*pi) by this call, so repeated
// calls produce a continuous, click-free tone across block boundaries.
void generateSineBlock(float* interleavedOut,
                       std::size_t numFrames,
                       std::size_t numChannels,
                       std::size_t targetChannel,
                       double sampleRate,
                       double frequencyHz,
                       float amplitude,
                       double& phase);

} // namespace mc::engine
