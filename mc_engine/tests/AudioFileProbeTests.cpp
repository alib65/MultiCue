#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "WaveTestUtil.h"
#include "mc/engine/AudioFileProbe.h"

using mc::engine::probeAudioFile;
using namespace mc::engine::test;

TEST_CASE("probeAudioFile reports channels/rate/duration without decoding audio", "[engine][probe]") {
    std::vector<float> samples(48000, 0.1f); // 1 s mono at 48 kHz
    const auto path = makeTempWavPath("probe");
    writeMonoWav(path, samples, 48000);

    const auto info = probeAudioFile(path);
    REQUIRE(info.valid);
    REQUIRE(info.channels == 1);
    REQUIRE(info.sampleRate == 48000.0);
    REQUIRE(info.durationSeconds == Catch::Approx(1.0).margin(1e-6));
}

TEST_CASE("probeAudioFile reports an error for a missing file", "[engine][probe]") {
    const auto info = probeAudioFile("/no/such/file.wav");
    REQUIRE_FALSE(info.valid);
    REQUIRE_FALSE(info.error.empty());
}
