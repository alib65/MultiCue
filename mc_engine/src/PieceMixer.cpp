#include "mc/engine/PieceMixer.h"

#include <algorithm>
#include <cmath>

namespace mc::engine {

PieceMixer::PieceMixer(std::size_t numTracks, std::size_t numOutputs, double sampleRate, std::size_t maxBlockFrames)
    : numTracks_(numTracks),
      numOutputs_(numOutputs),
      sampleRate_(sampleRate),
      maxBlockFrames_(maxBlockFrames),
      matrix_(GainMatrix::identityDirect(numTracks, numOutputs)),
      targetGainLinear_(numTracks, 1.0f),
      currentGainLinear_(numTracks, 1.0f),
      muted_(numTracks, false),
      solo_(numTracks, false),
      masterLimiter_(sampleRate, numOutputs, 5.0) {
    envelopes_.reserve(numTracks);
    for (std::size_t i = 0; i < numTracks; ++i) {
        envelopes_.emplace_back(sampleRate);
    }
    trackScratch_.assign(numTracks, std::vector<float>(maxBlockFrames, 0.0f));
}

void PieceMixer::setGainMatrix(GainMatrix matrix) {
    matrix_ = std::move(matrix);
}

void PieceMixer::setTrackGainDb(std::size_t track, float db) {
    if (track >= numTracks_) {
        return;
    }
    targetGainLinear_[track] = (db <= -60.0f) ? 0.0f : std::pow(10.0f, db / 20.0f);
}

void PieceMixer::setTrackMuted(std::size_t track, bool muted) {
    if (track >= numTracks_) {
        return;
    }
    muted_[track] = muted;
}

void PieceMixer::setTrackSolo(std::size_t track, bool solo) {
    if (track >= numTracks_) {
        return;
    }
    solo_[track] = solo;
}

void PieceMixer::start() {
    for (auto& env : envelopes_) {
        env.trigger(Envelope::Action::Start);
    }
}

void PieceMixer::stop() {
    for (auto& env : envelopes_) {
        env.trigger(Envelope::Action::Stop);
    }
}

bool PieceMixer::isSilent() const {
    return std::all_of(envelopes_.begin(), envelopes_.end(), [](const Envelope& e) { return e.isIdle(); });
}

std::size_t PieceMixer::processBlock(const std::vector<SampleSource*>& tracks, float* outInterleaved, std::size_t numFrames) {
    numFrames = std::min(numFrames, maxBlockFrames_);
    std::fill(outInterleaved, outInterleaved + (numFrames * numOutputs_), 0.0f);

    std::size_t shortfall = 0;
    const bool anySolo = std::any_of(solo_.begin(), solo_.end(), [](bool s) { return s; });

    for (std::size_t t = 0; t < numTracks_ && t < tracks.size(); ++t) {
        auto& scratch = trackScratch_[t];
        const std::size_t got = tracks[t]->read(scratch.data(), numFrames);
        if (got < numFrames) {
            std::fill(scratch.begin() + static_cast<long>(got), scratch.begin() + static_cast<long>(numFrames), 0.0f);
            shortfall += (numFrames - got);
        }

        const bool audible = !muted_[t] && (!anySolo || solo_[t]);
        const float target = audible ? targetGainLinear_[t] : 0.0f;
        const float smoothCoeff = static_cast<float>(std::exp(-1.0 / (0.05 * sampleRate_)));

        for (std::size_t frame = 0; frame < numFrames; ++frame) {
            currentGainLinear_[t] += (target - currentGainLinear_[t]) * (1.0f - smoothCoeff);
            const float env = envelopes_[t].nextSample();
            const float amp = scratch[frame] * currentGainLinear_[t] * env;
            if (amp == 0.0f) {
                continue;
            }
            for (std::size_t o = 0; o < numOutputs_; ++o) {
                const float g = matrix_.gain(t, o);
                if (g != 0.0f) {
                    outInterleaved[(frame * numOutputs_) + o] += amp * g;
                }
            }
        }
    }

    masterLimiter_.process(outInterleaved, numFrames);
    return shortfall;
}

} // namespace mc::engine
