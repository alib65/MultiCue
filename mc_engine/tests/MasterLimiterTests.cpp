#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

#include "mc/engine/MasterLimiter.h"

using mc::engine::MasterLimiter;

namespace {
float dbToLinear(float db) {
    return std::pow(10.0f, db / 20.0f);
}
} // namespace

TEST_CASE("MasterLimiter passes a below-ceiling signal through at unity master gain", "[engine][limiter]") {
    constexpr double sr = 48000.0;
    MasterLimiter lim(sr, 1, 5.0);
    lim.setMasterGainDb(0.0f);
    lim.setCeilingDb(-0.5f);

    constexpr std::size_t n = 4000; // well past the ~240-frame lookahead
    std::vector<float> buf(n, 0.1f);
    lim.process(buf.data(), n);

    for (std::size_t i = n - 500; i < n; ++i) {
        REQUIRE(buf[i] == Catch::Approx(0.1f).margin(1e-4));
    }
}

TEST_CASE("MasterLimiter applies the configured master gain when below ceiling", "[engine][limiter]") {
    constexpr double sr = 48000.0;
    MasterLimiter lim(sr, 1, 5.0);
    lim.setMasterGainDb(-6.0f);
    lim.setCeilingDb(-0.5f);

    // Master gain is smoothed with a ~0.1 s time constant (matching the
    // reference's Lag.kr(amp, 0.1)); run long enough for it to fully settle
    // before checking the tail -- a few seconds, not a handful of ms.
    constexpr std::size_t n = static_cast<std::size_t>(sr * 3);
    std::vector<float> buf(n, 0.1f);
    lim.process(buf.data(), n);

    const float expected = 0.1f * dbToLinear(-6.0f);
    for (std::size_t i = n - 500; i < n; ++i) {
        REQUIRE(buf[i] == Catch::Approx(expected).margin(1e-3));
    }
}

TEST_CASE("MasterLimiter clamps a sustained over-ceiling signal to the ceiling", "[engine][limiter]") {
    constexpr double sr = 48000.0;
    MasterLimiter lim(sr, 1, 5.0);
    lim.setMasterGainDb(0.0f);
    lim.setCeilingDb(-6.0f);

    constexpr std::size_t n = 6000;
    std::vector<float> buf(n, 1.0f);
    lim.process(buf.data(), n);

    const float ceiling = dbToLinear(-6.0f);
    for (std::size_t i = n - 500; i < n; ++i) {
        REQUIRE(buf[i] <= ceiling + 1e-4f);
        REQUIRE(buf[i] == Catch::Approx(ceiling).margin(1e-3));
    }
}

TEST_CASE("MasterLimiter never lets any sample exceed the ceiling, even during the attack transient", "[engine][limiter]") {
    constexpr double sr = 48000.0;
    MasterLimiter lim(sr, 1, 5.0);
    lim.setMasterGainDb(0.0f);
    lim.setCeilingDb(-6.0f);
    const float ceiling = dbToLinear(-6.0f);

    constexpr std::size_t n = 2000;
    std::vector<float> buf(n, 1.0f);
    lim.process(buf.data(), n);

    for (float v : buf) {
        REQUIRE(v <= ceiling + 1e-4f);
    }
}

TEST_CASE("Disabling the limiter leaves an over-ceiling signal untouched (aside from master gain)", "[engine][limiter]") {
    constexpr double sr = 48000.0;
    MasterLimiter lim(sr, 1, 5.0);
    lim.setMasterGainDb(0.0f);
    lim.setCeilingDb(-6.0f);
    lim.setEnabled(false);

    constexpr std::size_t n = 100;
    std::vector<float> buf(n, 1.0f);
    lim.process(buf.data(), n);

    for (float v : buf) {
        REQUIRE(v == Catch::Approx(1.0f).margin(1e-4));
    }
}
