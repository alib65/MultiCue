#pragma once

#include <cstddef>
#include <vector>

namespace mc::engine {

// A fixed-size sources x outputs gain matrix, per docs/ARCHITECTURE.md §3.3.
// This is deliberately dumb storage (row-major floats) -- the listening
// formats that *compute* interesting matrices (Ambisonics, VBAP, binaural,
// ...) live in mc_dsp from M3 onward. Direct mode (M1/M2) builds one with
// identityDirect() below: track i -> output (offset + i) at gain 1.
class GainMatrix {
public:
    GainMatrix() = default;
    GainMatrix(std::size_t numSources, std::size_t numOutputs);

    std::size_t numSources() const noexcept { return numSources_; }
    std::size_t numOutputs() const noexcept { return numOutputs_; }

    float gain(std::size_t source, std::size_t output) const noexcept;
    void setGain(std::size_t source, std::size_t output, float value);

    // One output per source, in order, starting at `firstOutput`; sources
    // beyond the available outputs are silently dropped (the caller is
    // expected to surface that as a status message, as the reference does
    // for "this piece has N tracks but only M outputs").
    static GainMatrix identityDirect(std::size_t numSources, std::size_t numOutputs, std::size_t firstOutput = 0);

private:
    std::size_t numSources_ = 0;
    std::size_t numOutputs_ = 0;
    std::vector<float> gains_; // row-major: source * numOutputs_ + output
};

} // namespace mc::engine
