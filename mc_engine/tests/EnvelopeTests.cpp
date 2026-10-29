#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "mc/engine/Envelope.h"

using mc::engine::Envelope;

TEST_CASE("Envelope is silent and idle before any trigger", "[engine][envelope]") {
    Envelope env(48000.0);
    REQUIRE(env.isIdle());
    REQUIRE(env.nextSample() == 0.0f);
}

TEST_CASE("Envelope rises monotonically and click-free to 1 after Start", "[engine][envelope]") {
    Envelope env(48000.0, 0.005, 0.08);
    env.trigger(Envelope::Action::Start);

    float prev = 0.0f;
    bool reachedOne = false;
    for (int i = 0; i < 48000; ++i) {
        const float v = env.nextSample();
        REQUIRE(v >= prev); // monotonic rise: never a downward jump during attack
        REQUIRE((v - prev) <= 0.05f); // no single-sample jump larger than 5% of full scale
        prev = v;
        if (v >= 1.0f) {
            reachedOne = true;
            break;
        }
    }
    REQUIRE(reachedOne);
}

TEST_CASE("Envelope falls to (near) zero and reports finished after Stop", "[engine][envelope]") {
    Envelope env(48000.0, 0.005, 0.08);
    env.trigger(Envelope::Action::Start);
    for (int i = 0; i < 1000; ++i) {
        env.nextSample();
    }
    env.trigger(Envelope::Action::Stop);

    float prev = 1.0f;
    bool finished = false;
    for (int i = 0; i < 48000; ++i) {
        const float v = env.nextSample();
        REQUIRE(v <= prev + 1e-6f); // monotonic fall during release
        prev = v;
        if (env.isFinished()) {
            finished = true;
            break;
        }
    }
    REQUIRE(finished);
    REQUIRE(prev < 0.01f);
}

TEST_CASE("A Start retriggered mid-release has no discontinuity", "[engine][envelope]") {
    Envelope env(48000.0, 0.005, 0.08);
    env.trigger(Envelope::Action::Start);
    for (int i = 0; i < 1000; ++i) {
        env.nextSample();
    }
    env.trigger(Envelope::Action::Stop);
    float beforeRetrigger = 1.0f;
    for (int i = 0; i < 200; ++i) {
        beforeRetrigger = env.nextSample();
    }
    env.trigger(Envelope::Action::Start);
    const float afterRetrigger = env.nextSample();
    // The very next sample after re-triggering Start must be close to where
    // the release had gotten to, not a jump back to 0 or up to 1.
    REQUIRE(std::abs(afterRetrigger - beforeRetrigger) < 0.05f);
}
