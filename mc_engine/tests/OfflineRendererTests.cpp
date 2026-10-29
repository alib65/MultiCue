#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

#include "WaveTestUtil.h"
#include "mc/engine/OfflineRenderer.h"

using mc::engine::OfflineRenderRequest;
using mc::engine::renderOffline;
using namespace mc::engine::test;

TEST_CASE("Direct-mode render is bit-exact in the sustained region", "[engine][offline]") {
    constexpr int sr = 48000;
    constexpr std::size_t n = 24000; // 0.5 s
    std::vector<float> track0(n), track1(n);
    for (std::size_t i = 0; i < n; ++i) {
        track0[i] = std::sin(0.02f * static_cast<float>(i));
        track1[i] = std::cos(0.015f * static_cast<float>(i)) * 0.6f;
    }
    const auto path0 = makeTempWavPath("direct0");
    const auto path1 = makeTempWavPath("direct1");
    writeMonoWav(path0, track0, sr);
    writeMonoWav(path1, track1, sr);

    OfflineRenderRequest req;
    req.trackPaths = {path0, path1};
    req.numOutputs = 2;
    req.sampleRate = sr;
    req.masterGainDb = 0.0f;
    req.limiterEnabled = false; // isolate routing/gain exactness from limiting
    req.outputPath = makeTempWavPath("direct_out");

    const auto result = renderOffline(req);
    REQUIRE(result.success);

    const auto out = readWav(req.outputPath);
    REQUIRE(out.channels == 2);

    // Skip the ~5 ms attack at the start and anything near the stop/release
    // at the end; the middle must reproduce the source files exactly,
    // since 0 dB gain + no limiter + matching sample rate is a pure
    // passthrough (docs/ARCHITECTURE.md §9 "direct mode is bit-exact").
    for (sf_count_t f = 2000; f < 20000; ++f) {
        REQUIRE(out.at(f, 0) == track0[static_cast<std::size_t>(f)]);
        REQUIRE(out.at(f, 1) == track1[static_cast<std::size_t>(f)]);
    }
}

TEST_CASE("Multiple tracks of a piece start on the exact same frame and stay sample-locked", "[engine][offline]") {
    constexpr int sr = 48000;
    constexpr std::size_t n = 24000;
    std::vector<float> identical(n);
    for (std::size_t i = 0; i < n; ++i) {
        identical[i] = std::sin(0.03f * static_cast<float>(i)) * 0.5f;
    }
    const auto path0 = makeTempWavPath("lock0");
    const auto path1 = makeTempWavPath("lock1");
    const auto path2 = makeTempWavPath("lock2");
    writeMonoWav(path0, identical, sr);
    writeMonoWav(path1, identical, sr);
    writeMonoWav(path2, identical, sr);

    OfflineRenderRequest req;
    req.trackPaths = {path0, path1, path2};
    req.numOutputs = 3;
    req.sampleRate = sr;
    req.limiterEnabled = false;
    req.outputPath = makeTempWavPath("lock_out");

    const auto result = renderOffline(req);
    REQUIRE(result.success);

    const auto out = readWav(req.outputPath);
    REQUIRE(out.channels == 3);
    for (sf_count_t f = 0; f < out.frames; ++f) {
        REQUIRE(out.at(f, 0) == out.at(f, 1));
        REQUIRE(out.at(f, 1) == out.at(f, 2));
    }
}

TEST_CASE("Render output starts from silence and ramps up (click-free attack)", "[engine][offline]") {
    constexpr int sr = 48000;
    constexpr std::size_t n = 4800;
    std::vector<float> constant(n, 1.0f);
    const auto path = makeTempWavPath("attack");
    writeMonoWav(path, constant, sr);

    OfflineRenderRequest req;
    req.trackPaths = {path};
    req.numOutputs = 1;
    req.sampleRate = sr;
    req.limiterEnabled = false;
    req.outputPath = makeTempWavPath("attack_out");

    const auto result = renderOffline(req);
    REQUIRE(result.success);

    const auto out = readWav(req.outputPath);
    REQUIRE(out.at(0, 0) < 0.05f);

    // Monotonic rise, no overshoot above the source amplitude, no
    // backwards jump anywhere in the attack region.
    float prev = out.at(0, 0);
    bool reachedFull = false;
    for (sf_count_t f = 1; f < 500; ++f) {
        const float v = out.at(f, 0);
        REQUIRE(v >= prev - 1e-6f);
        REQUIRE(v <= 1.0001f);
        prev = v;
        if (v > 0.999f) {
            reachedFull = true;
        }
    }
    REQUIRE(reachedFull);
}

TEST_CASE("renderOffline reports an error for a missing track file instead of crashing", "[engine][offline]") {
    OfflineRenderRequest req;
    req.trackPaths = {"/no/such/file.wav"};
    req.numOutputs = 1;
    req.outputPath = makeTempWavPath("missing_out");

    const auto result = renderOffline(req);
    REQUIRE_FALSE(result.success);
    REQUIRE_FALSE(result.error.empty());
}
