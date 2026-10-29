#include "mc/engine/SineGenerator.h"

#include <cmath>

namespace mc::engine {

namespace {
constexpr double kTwoPi = 6.283185307179586476925286766559;
}

void generateSineBlock(float* interleavedOut,
                       std::size_t numFrames,
                       std::size_t numChannels,
                       std::size_t targetChannel,
                       double sampleRate,
                       double frequencyHz,
                       float amplitude,
                       double& phase) {
    if (numChannels == 0 || sampleRate <= 0.0) {
        return;
    }
    const double increment = kTwoPi * frequencyHz / sampleRate;
    for (std::size_t frame = 0; frame < numFrames; ++frame) {
        const float sample = (targetChannel < numChannels)
            ? static_cast<float>(std::sin(phase) * amplitude)
            : 0.0f;
        for (std::size_t ch = 0; ch < numChannels; ++ch) {
            interleavedOut[frame * numChannels + ch] = (ch == targetChannel) ? sample : 0.0f;
        }
        phase += increment;
        if (phase >= kTwoPi) {
            phase -= kTwoPi;
        }
    }
}

} // namespace mc::engine
