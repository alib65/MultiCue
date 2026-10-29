#pragma once

#include <cstddef>

namespace mc::engine {

// A click-free attack/release envelope for voice start/stop/pause/seek, per
// docs/ARCHITECTURE.md §3.4 ("every level change, start, stop, pause and
// seek uses a short fade"). Pure state machine: no allocation, no I/O.
//
// trigger(Start) ramps 0 -> 1 over `attackSeconds`. trigger(Stop) ramps the
// current value -> 0 over `releaseSeconds` and reports isFinished() once it
// reaches (approximately) zero, so the caller knows it is safe to free/reset
// the voice. trigger(Start) while releasing resumes upward from wherever the
// release had gotten to, so a rapid stop/restart never clicks.
class Envelope {
public:
    enum class Action { Start, Stop };

    Envelope(double sampleRate, double attackSeconds = 0.005, double releaseSeconds = 0.08);

    void trigger(Action action);

    // Advances the envelope by one sample and returns its new value in [0, 1].
    float nextSample();

    // True once a Stop has driven the envelope close enough to zero that the
    // voice it gates may be considered silent and reclaimed.
    bool isFinished() const noexcept;

    bool isIdle() const noexcept;

private:
    enum class State { Idle, Attacking, Sustained, Releasing };

    double sampleRate_;
    double attackSeconds_;
    double releaseSeconds_;
    float value_ = 0.0f;
    State state_ = State::Idle;
};

} // namespace mc::engine
