#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

#include "WaveTestUtil.h"
#include "mc/engine/FileSampleSource.h"

using mc::engine::FileSampleSource;
using namespace mc::engine::test;

TEST_CASE("FileSampleSource passthrough is bit-exact when rates match", "[engine][filesource]") {
    std::vector<float> samples(2000);
    for (std::size_t i = 0; i < samples.size(); ++i) {
        samples[i] = std::sin(0.01f * static_cast<float>(i));
    }
    const auto path = makeTempWavPath("passthrough");
    writeMonoWav(path, samples, 48000);

    FileSampleSource src(path, 0, 48000.0);
    REQUIRE(src.isValid());

    std::vector<float> readBack(samples.size());
    const auto got = src.read(readBack.data(), readBack.size());
    REQUIRE(got == samples.size());
    for (std::size_t i = 0; i < samples.size(); ++i) {
        REQUIRE(readBack[i] == samples[i]);
    }
}

TEST_CASE("FileSampleSource reports atEnd after the file is fully consumed", "[engine][filesource]") {
    std::vector<float> samples(100, 0.5f);
    const auto path = makeTempWavPath("atend");
    writeMonoWav(path, samples, 48000);

    FileSampleSource src(path, 0, 48000.0);
    std::vector<float> buf(200);
    const auto got = src.read(buf.data(), buf.size());
    REQUIRE(got == 100);
    REQUIRE(src.atEnd());
}

TEST_CASE("FileSampleSource resampling produces roughly the expected frame count", "[engine][filesource]") {
    std::vector<float> samples(4800, 1.0f); // 0.1 s at 48 kHz
    const auto path = makeTempWavPath("resample");
    writeMonoWav(path, samples, 48000);

    FileSampleSource src(path, 0, 44100.0);
    REQUIRE(src.isValid());
    std::vector<float> buf(10000, -1.0f);
    const auto got = src.read(buf.data(), buf.size());
    // 0.1 s at 44100 Hz is ~4410 frames; allow generous margin for filter
    // ramp-up/ramp-down at the resampler's edges.
    REQUIRE(got > 4000);
    REQUIRE(got < 4800);
}

TEST_CASE("FileSampleSource on a missing file is invalid, not crashing", "[engine][filesource]") {
    FileSampleSource src("/no/such/file/at/all.wav", 0, 48000.0);
    REQUIRE_FALSE(src.isValid());
    REQUIRE_FALSE(src.lastError().empty());
}
