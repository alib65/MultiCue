#include "mc/engine/Envelope.h"

#include <cmath>

namespace mc::engine {

Envelope::Envelope(double sampleRate, double attackSeconds, double releaseSeconds)
    : sampleRate_(sampleRate), attackSeconds_(attackSeconds), releaseSeconds_(releaseSeconds) {}

void Envelope::trigger(Action action) {
    if (action == Action::Start) {
        state_ = State::Attacking;
    } else {
        state_ = State::Releasing;
    }
}

float Envelope::nextSample() {
    switch (state_) {
        case State::Idle:
            value_ = 0.0f;
            break;
        case State::Attacking: {
            const double step = (attackSeconds_ > 0.0) ? (1.0 / (attackSeconds_ * sampleRate_)) : 1.0;
            value_ += static_cast<float>(step);
            if (value_ >= 1.0f) {
                value_ = 1.0f;
                state_ = State::Sustained;
            }
            break;
        }
        case State::Sustained:
            value_ = 1.0f;
            break;
        case State::Releasing: {
            // Exponential-ish decay: multiply towards zero each sample so a
            // release restarted mid-fade (Start while Releasing) has a
            // continuous slope, not a kink.
            const double timeConstant = (releaseSeconds_ > 0.0) ? releaseSeconds_ : (1.0 / sampleRate_);
            const double coeff = std::exp(-1.0 / (timeConstant * sampleRate_));
            value_ = static_cast<float>(value_ * coeff);
            if (value_ < 0.0005f) {
                value_ = 0.0f;
                state_ = State::Idle;
            }
            break;
        }
    }
    return value_;
}

bool Envelope::isFinished() const noexcept {
    return state_ == State::Idle;
}

bool Envelope::isIdle() const noexcept {
    return state_ == State::Idle && value_ == 0.0f;
}

} // namespace mc::engine
