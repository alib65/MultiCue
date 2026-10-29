#pragma once

#include <cstddef>
#include <vector>

#include "mc/engine/Envelope.h"
#include "mc/engine/GainMatrix.h"
#include "mc/engine/MasterLimiter.h"
#include "mc/engine/SampleSource.h"

namespace mc::engine {

// Mixes numTracks mono SampleSources through a GainMatrix into an
// interleaved numOutputs-channel buffer, through the master gain/limiter.
// This is the shared, allocation-free-at-process-time core described in
// docs/ARCHITECTURE.md §3.7: the realtime engine drives it from
// ring-buffer-backed sources inside the audio callback, and the offline
// renderer drives the identical code from direct file sources -- so
// "direct mode is bit-exact" and "fades/seeks are click-free" are properties
// of this one class, tested once, not two parallel implementations that
// could disagree.
//
// All buffers are sized at construction from maxBlockFrames; processBlock()
// itself never allocates, so it is safe to call from the audio thread.
class PieceMixer {
public:
    PieceMixer(std::size_t numTracks, std::size_t numOutputs, double sampleRate, std::size_t maxBlockFrames);

    void setGainMatrix(GainMatrix matrix);
    void setTrackGainDb(std::size_t track, float db);
    void setTrackMuted(std::size_t track, bool muted);
    void setTrackSolo(std::size_t track, bool solo);

    // Triggers the start/stop envelope on every track simultaneously, so a
    // multi-file piece starts and stops sample-locked across all its tracks
    // (docs/ARCHITECTURE.md §3.6).
    void start();
    void stop();

    // True once every track's envelope has fully released after stop().
    bool isSilent() const;

    MasterLimiter& masterLimiter() { return masterLimiter_; }

    // Pulls up to numFrames from each of `tracks` (tracks.size() must equal
    // numTracks passed to the constructor), mixes and writes
    // numFrames * numOutputs interleaved floats to `outInterleaved`
    // (must be at least that large). Returns the number of input frames
    // that were short of a full read across any track (an underrun/EOF
    // signal for the caller), summed per block.
    std::size_t processBlock(const std::vector<SampleSource*>& tracks, float* outInterleaved, std::size_t numFrames);

private:
    std::size_t numTracks_;
    std::size_t numOutputs_;
    double sampleRate_;
    std::size_t maxBlockFrames_;

    GainMatrix matrix_;
    std::vector<float> targetGainLinear_;
    std::vector<float> currentGainLinear_;
    std::vector<bool> muted_;
    std::vector<bool> solo_;
    std::vector<Envelope> envelopes_;
    std::vector<std::vector<float>> trackScratch_;

    MasterLimiter masterLimiter_;

    void recomputeAudibility();
};

} // namespace mc::engine
