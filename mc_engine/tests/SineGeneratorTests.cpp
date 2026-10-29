#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

#include "mc/engine/SineGenerator.h"

using mc::engine::generateSineBlock;

TEST_CASE("generateSineBlock silences channels other than the target", "[engine][sine]") {
    constexpr std::size_t frames = 8;
    constexpr std::size_t channels = 4;
    constexpr std::size_t target = 2;
    std::vector<float> buf(frames * channels, -1.0f);
    double phase = 0.0;

    generateSineBlock(buf.data(), frames, channels, target, 48000.0, 1000.0, 0.5f, phase);

    for (std::size_t f = 0; f < frames; ++f) {
        for (std::size_t ch = 0; ch < channels; ++ch) {
            if (ch != target) {
                REQUIRE(buf[f * channels + ch] == 0.0f);
            }
        }
    }
}

TEST_CASE("generateSineBlock amplitude stays within the requested peak", "[engine][sine]") {
    constexpr std::size_t frames = 4800;
    constexpr std::size_t channels = 1;
    std::vector<float> buf(frames * channels);
    double phase = 0.0;

    generateSineBlock(buf.data(), frames, channels, 0, 48000.0, 1000.0, 0.25f, phase);

    for (float s : buf) {
        REQUIRE(std::abs(s) <= 0.25f + 1e-6f);
    }
}

TEST_CASE("generateSineBlock phase is continuous across successive blocks", "[engine][sine]") {
    // A tone generated in one call must be identical, sample for sample, to
    // the same tone generated across two back-to-back calls that share the
    // same phase accumulator -- this is the property the real-time callback
    // depends on for a click-free tone across buffer boundaries.
    constexpr std::size_t totalFrames = 2000;
    constexpr std::size_t splitAt = 777;

    std::vector<float> whole(totalFrames);
    double wholePhase = 0.0;
    generateSineBlock(whole.data(), totalFrames, 1, 0, 48000.0, 1000.0, 1.0f, wholePhase);

    std::vector<float> split(totalFrames);
    double splitPhase = 0.0;
    generateSineBlock(split.data(), splitAt, 1, 0, 48000.0, 1000.0, 1.0f, splitPhase);
    generateSineBlock(split.data() + splitAt, totalFrames - splitAt, 1, 0, 48000.0, 1000.0, 1.0f, splitPhase);

    for (std::size_t i = 0; i < totalFrames; ++i) {
        REQUIRE(whole[i] == Catch::Approx(split[i]).margin(1e-6));
    }
}

TEST_CASE("generateSineBlock no-op on zero channels", "[engine][sine]") {
    double phase = 0.0;
    // Must not crash or write out of bounds when there is nothing to write.
    generateSineBlock(nullptr, 100, 0, 0, 48000.0, 1000.0, 1.0f, phase);
    REQUIRE(phase == 0.0);
}
