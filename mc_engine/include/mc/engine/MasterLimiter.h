#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

namespace mc::engine {

// Master gain (smoothed) plus a lookahead brick-wall peak limiter, acting
// only above the configured ceiling -- matching the reference's "safety
// limiter: acts only if an output would go above -0.5 dBFS" behaviour
// (docs/ARCHITECTURE.md §0). This stage uses *sample* peak, same as the
// reference; the true-peak (4x oversampled) upgrade is deliberately
// deferred to M4 (see docs/ROADMAP.md) where it is built once, shared with
// loudness analysis, and swapped in here instead of being written twice.
//
// Limits each channel independently (as the reference's per-channel
// `Limiter.ar` does), not as a single linked multichannel detector.
//
// process() is real-time safe: every buffer it touches is sized once at
// construction (lookaheadFrames_ is fixed for the object's lifetime), so
// there is no allocation on the audio thread.
class MasterLimiter {
public:
    MasterLimiter(double sampleRate, std::size_t numChannels, double lookaheadMs = 5.0);

    void setMasterGainDb(float db);
    void setCeilingDb(float db);
    void setEnabled(bool enabled) noexcept { enabled_ = enabled; }

    // Processes `numFrames` interleaved frames of `numChannels_` in place.
    void process(float* interleaved, std::size_t numFrames);

private:
    // Fixed-capacity monotonic-decreasing deque (sliding-window maximum),
    // backed by a preallocated ring so steady-state operation never
    // allocates. Capacity equals the lookahead length: a window can never
    // hold more live entries than that.
    struct MonotonicMaxWindow {
        std::vector<std::pair<std::size_t, float>> ring;
        std::size_t head = 0; // first valid entry
        std::size_t tail = 0; // one past the last valid entry
        std::size_t count = 0;

        explicit MonotonicMaxWindow(std::size_t capacity) : ring(capacity) {}

        void push(std::size_t index, float value) {
            const std::size_t capacity = ring.size();
            while (count > 0 && ring[(tail + capacity - 1) % capacity].second <= value) {
                tail = (tail + capacity - 1) % capacity;
                --count;
            }
            ring[tail] = {index, value};
            tail = (tail + 1) % capacity;
            ++count;
        }

        void evictOlderThan(std::size_t minIndex) {
            const std::size_t capacity = ring.size();
            while (count > 0 && ring[head].first < minIndex) {
                head = (head + 1) % capacity;
                --count;
            }
        }

        float currentMax() const { return (count > 0) ? ring[head].second : 0.0f; }
    };

    struct ChannelState {
        std::vector<float> delayLine;
        std::size_t writePos = 0;
        MonotonicMaxWindow maxWindow;
        std::size_t sampleIndex = 0;
        float smoothedGain = 1.0f;

        explicit ChannelState(std::size_t lookaheadFrames) : delayLine(lookaheadFrames, 0.0f), maxWindow(std::max<std::size_t>(lookaheadFrames, 1)) {}
    };

    double sampleRate_;
    std::size_t numChannels_;
    std::size_t lookaheadFrames_;
    float masterGain_ = 1.0f;
    float smoothedMasterGain_ = 1.0f;
    float ceilingLinear_ = 0.944f; // ~ -0.5 dBFS, matching the reference's default
    bool enabled_ = true;

    std::vector<ChannelState> channels_;
};

} // namespace mc::engine
