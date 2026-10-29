#include "mc/engine/GainMatrix.h"

#include <cassert>

namespace mc::engine {

GainMatrix::GainMatrix(std::size_t numSources, std::size_t numOutputs)
    : numSources_(numSources), numOutputs_(numOutputs), gains_(numSources * numOutputs, 0.0f) {}

float GainMatrix::gain(std::size_t source, std::size_t output) const noexcept {
    if (source >= numSources_ || output >= numOutputs_) {
        return 0.0f;
    }
    return gains_[(source * numOutputs_) + output];
}

void GainMatrix::setGain(std::size_t source, std::size_t output, float value) {
    if (source >= numSources_ || output >= numOutputs_) {
        return;
    }
    gains_[(source * numOutputs_) + output] = value;
}

GainMatrix GainMatrix::identityDirect(std::size_t numSources, std::size_t numOutputs, std::size_t firstOutput) {
    GainMatrix m(numSources, numOutputs);
    for (std::size_t i = 0; i < numSources; ++i) {
        const std::size_t out = firstOutput + i;
        if (out < numOutputs) {
            m.setGain(i, out, 1.0f);
        }
    }
    return m;
}

} // namespace mc::engine
