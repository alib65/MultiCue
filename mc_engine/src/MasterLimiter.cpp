#include "mc/engine/MasterLimiter.h"

#include <algorithm>
#include <cmath>

namespace mc::engine {

MasterLimiter::MasterLimiter(double sampleRate, std::size_t numChannels, double lookaheadMs)
    : sampleRate_(sampleRate),
      numChannels_(numChannels),
      lookaheadFrames_(std::max<std::size_t>(1, static_cast<std::size_t>(lookaheadMs * 0.001 * sampleRate))) {
    channels_.reserve(numChannels_);
    for (std::size_t i = 0; i < numChannels_; ++i) {
        channels_.emplace_back(lookaheadFrames_);
    }
}

void MasterLimiter::setMasterGainDb(float db) {
    masterGain_ = (db <= -120.0f) ? 0.0f : std::pow(10.0f, db / 20.0f);
}

void MasterLimiter::setCeilingDb(float db) {
    ceilingLinear_ = std::pow(10.0f, db / 20.0f);
}

void MasterLimiter::process(float* interleaved, std::size_t numFrames) {
    // Master gain is smoothed over roughly 10 ms, matching the reference's
    // Lag.kr(amp, 0.1) on the master synth.
    const float gainSmoothCoeff = static_cast<float>(std::exp(-1.0 / (0.1 * sampleRate_)));
    const float releaseCoeff = static_cast<float>(std::exp(-1.0 / (0.05 * sampleRate_)));

    for (std::size_t frame = 0; frame < numFrames; ++frame) {
        smoothedMasterGain_ += (masterGain_ - smoothedMasterGain_) * (1.0f - gainSmoothCoeff);

        for (std::size_t ch = 0; ch < numChannels_; ++ch) {
            ChannelState& cs = channels_[ch];
            const std::size_t idx = (frame * numChannels_) + ch;
            const float gained = interleaved[idx] * smoothedMasterGain_;

            if (enabled_) {
                cs.maxWindow.push(cs.sampleIndex, std::abs(gained));
                const std::size_t minIndex = (cs.sampleIndex + 1 >= lookaheadFrames_) ? (cs.sampleIndex + 1 - lookaheadFrames_) : 0;
                cs.maxWindow.evictOlderThan(minIndex);
                ++cs.sampleIndex;

                const float windowMax = cs.maxWindow.currentMax();
                const float desiredGain = (windowMax > ceilingLinear_) ? (ceilingLinear_ / windowMax) : 1.0f;
                if (desiredGain <= cs.smoothedGain) {
                    cs.smoothedGain = desiredGain;
                } else {
                    cs.smoothedGain += (desiredGain - cs.smoothedGain) * (1.0f - releaseCoeff);
                }

                const float delayed = cs.delayLine[cs.writePos];
                cs.delayLine[cs.writePos] = gained;
                cs.writePos = (cs.writePos + 1) % lookaheadFrames_;

                interleaved[idx] = delayed * cs.smoothedGain;
            } else {
                interleaved[idx] = gained;
            }
        }
    }
}

} // namespace mc::engine
