#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

#include "mc/engine/PieceMixer.h"
#include "mc/engine/SampleSource.h"

using mc::engine::GainMatrix;
using mc::engine::PieceMixer;
using mc::engine::SampleSource;

namespace {

// A SampleSource that always returns a constant value -- the simplest
// possible deterministic input for mixing tests.
class ConstantSource : public SampleSource {
public:
    explicit ConstantSource(float value) : value_(value) {}
    std::size_t read(float* out, std::size_t numFrames) override {
        std::fill(out, out + numFrames, value_);
        return numFrames;
    }
    bool atEnd() const override { return false; }

private:
    float value_;
};

} // namespace

TEST_CASE("PieceMixer routes track i to output i in direct mode", "[engine][piecemixer]") {
    constexpr double sr = 48000.0;
    PieceMixer mixer(2, 2, sr, 4096);
    mixer.masterLimiter().setEnabled(false);
    mixer.start();

    ConstantSource a(0.5f);
    ConstantSource b(0.25f);
    std::vector<SampleSource*> tracks{&a, &b};

    std::vector<float> out(4096 * 2);
    // Run long enough for the attack envelope (5 ms) to fully settle.
    for (int i = 0; i < 5; ++i) {
        mixer.processBlock(tracks, out.data(), 4096);
    }

    for (std::size_t f = 4000; f < 4096; ++f) {
        REQUIRE(out[(f * 2) + 0] == Catch::Approx(0.5f).margin(1e-3));
        REQUIRE(out[(f * 2) + 1] == Catch::Approx(0.25f).margin(1e-3));
    }
}

TEST_CASE("PieceMixer is silent before start() and ramps up smoothly, never overshooting", "[engine][piecemixer]") {
    constexpr double sr = 48000.0;
    PieceMixer mixer(1, 1, sr, 4096);
    mixer.masterLimiter().setEnabled(false);

    ConstantSource a(1.0f);
    std::vector<SampleSource*> tracks{&a};
    std::vector<float> out(4096);

    // Not started yet: must be silent.
    mixer.processBlock(tracks, out.data(), 4096);
    for (float v : out) {
        REQUIRE(v == 0.0f);
    }

    mixer.start();
    mixer.processBlock(tracks, out.data(), 4096);
    for (float v : out) {
        REQUIRE(v <= 1.0001f);
    }
    // First sample after start must still be close to 0, not a jump to 1.
    REQUIRE(out[0] < 0.1f);
}

TEST_CASE("Two tracks playing the same source stay sample-locked across a whole block", "[engine][piecemixer]") {
    constexpr double sr = 48000.0;
    PieceMixer mixer(2, 2, sr, 4096);
    mixer.masterLimiter().setEnabled(false);
    mixer.start();

    ConstantSource a(0.7f);
    ConstantSource b(0.7f);
    std::vector<SampleSource*> tracks{&a, &b};
    std::vector<float> out(4096 * 2);
    mixer.processBlock(tracks, out.data(), 4096);

    for (std::size_t f = 0; f < 4096; ++f) {
        REQUIRE(out[(f * 2) + 0] == out[(f * 2) + 1]);
    }
}

TEST_CASE("Muted track contributes silence, unmuted track unaffected", "[engine][piecemixer]") {
    constexpr double sr = 48000.0;
    PieceMixer mixer(2, 2, sr, 4096);
    mixer.masterLimiter().setEnabled(false);
    mixer.setTrackMuted(0, true);
    mixer.start();

    ConstantSource a(1.0f);
    ConstantSource b(1.0f);
    std::vector<SampleSource*> tracks{&a, &b};
    std::vector<float> out(4096 * 2);
    // Per-track gain is smoothed with a ~0.05 s time constant; run enough
    // blocks for the muted track's gain to fully settle to 0 before checking.
    for (int i = 0; i < 15; ++i) {
        mixer.processBlock(tracks, out.data(), 4096);
    }

    REQUIRE(out[(4000 * 2) + 0] == Catch::Approx(0.0f).margin(1e-4));
    REQUIRE(out[(4000 * 2) + 1] == Catch::Approx(1.0f).margin(1e-3));
}

TEST_CASE("Solo isolates the soloed track", "[engine][piecemixer]") {
    constexpr double sr = 48000.0;
    PieceMixer mixer(2, 2, sr, 4096);
    mixer.masterLimiter().setEnabled(false);
    mixer.setTrackSolo(1, true);
    mixer.start();

    ConstantSource a(1.0f);
    ConstantSource b(1.0f);
    std::vector<SampleSource*> tracks{&a, &b};
    std::vector<float> out(4096 * 2);
    for (int i = 0; i < 15; ++i) {
        mixer.processBlock(tracks, out.data(), 4096);
    }

    REQUIRE(out[(4000 * 2) + 0] == Catch::Approx(0.0f).margin(1e-4));
    REQUIRE(out[(4000 * 2) + 1] == Catch::Approx(1.0f).margin(1e-3));
}

TEST_CASE("setGainMatrix takes effect on the next processBlock call (live routing change)", "[engine][piecemixer]") {
    constexpr double sr = 48000.0;
    PieceMixer mixer(1, 2, sr, 4096);
    mixer.masterLimiter().setEnabled(false);
    mixer.start();

    ConstantSource a(1.0f);
    std::vector<SampleSource*> tracks{&a};
    std::vector<float> out(4096 * 2);
    for (int i = 0; i < 3; ++i) {
        mixer.processBlock(tracks, out.data(), 4096);
    }
    // Default identity routing: track 0 -> output 0.
    REQUIRE(out[(4000 * 2) + 0] == Catch::Approx(1.0f).margin(1e-3));
    REQUIRE(out[(4000 * 2) + 1] == Catch::Approx(0.0f).margin(1e-4));

    GainMatrix swapped(1, 2);
    swapped.setGain(0, 1, 1.0f); // route track 0 -> output 1 instead
    mixer.setGainMatrix(swapped);
    mixer.processBlock(tracks, out.data(), 4096);

    REQUIRE(out[(100 * 2) + 0] == Catch::Approx(0.0f).margin(1e-4));
    REQUIRE(out[(100 * 2) + 1] == Catch::Approx(1.0f).margin(1e-3));
}

TEST_CASE("Track and output meters reflect the post-fader signal", "[engine][piecemixer]") {
    constexpr double sr = 48000.0;
    PieceMixer mixer(1, 1, sr, 4096);
    mixer.masterLimiter().setEnabled(false);
    mixer.start();

    ConstantSource a(0.5f);
    std::vector<SampleSource*> tracks{&a};
    std::vector<float> out(4096);
    for (int i = 0; i < 5; ++i) {
        mixer.processBlock(tracks, out.data(), 4096);
    }

    REQUIRE(mixer.trackPeak(0) == Catch::Approx(0.5f).margin(1e-3));
    REQUIRE(mixer.trackRms(0) == Catch::Approx(0.5f).margin(1e-3));
    REQUIRE(mixer.outputPeak(0) == Catch::Approx(0.5f).margin(1e-3));
    REQUIRE(mixer.outputRms(0) == Catch::Approx(0.5f).margin(1e-3));
}

TEST_CASE("A muted track's meter reads 0", "[engine][piecemixer]") {
    constexpr double sr = 48000.0;
    PieceMixer mixer(1, 1, sr, 4096);
    mixer.masterLimiter().setEnabled(false);
    mixer.setTrackMuted(0, true);
    mixer.start();

    ConstantSource a(1.0f);
    std::vector<SampleSource*> tracks{&a};
    std::vector<float> out(4096);
    for (int i = 0; i < 15; ++i) {
        mixer.processBlock(tracks, out.data(), 4096);
    }
    REQUIRE(mixer.trackPeak(0) == Catch::Approx(0.0f).margin(1e-4));
}

TEST_CASE("isSilent reports true only after stop() and the release tail finishes", "[engine][piecemixer]") {
    constexpr double sr = 48000.0;
    PieceMixer mixer(1, 1, sr, 4096);
    mixer.masterLimiter().setEnabled(false);
    mixer.start();

    ConstantSource a(1.0f);
    std::vector<SampleSource*> tracks{&a};
    std::vector<float> out(4096);
    mixer.processBlock(tracks, out.data(), 4096);
    REQUIRE_FALSE(mixer.isSilent());

    mixer.stop();
    bool becameSilent = false;
    for (int i = 0; i < 20; ++i) {
        mixer.processBlock(tracks, out.data(), 4096);
        if (mixer.isSilent()) {
            becameSilent = true;
            break;
        }
    }
    REQUIRE(becameSilent);
}
