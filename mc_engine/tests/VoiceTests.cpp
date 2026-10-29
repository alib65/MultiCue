#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <thread>
#include <vector>

#include "WaveTestUtil.h"
#include "mc/engine/Voice.h"

using mc::engine::Voice;
using namespace mc::engine::test;

TEST_CASE("Voice reports an invalid file without starting a reader thread", "[engine][voice]") {
    Voice v("/no/such/file.wav", 0, 48000.0, 0.0, 4096);
    REQUIRE_FALSE(v.isValidFile());
    REQUIRE_FALSE(v.lastError().empty());
}

TEST_CASE("Voice streams a file's samples bit-exact through its ring buffer", "[engine][voice]") {
    std::vector<float> samples(20000);
    for (std::size_t i = 0; i < samples.size(); ++i) {
        samples[i] = (i % 7 == 0) ? 1.0f : -0.25f; // distinctive, easy-to-check pattern
    }
    const auto path = makeTempWavPath("voice_stream");
    writeMonoWav(path, samples, 48000);

    Voice v(path, 0, 48000.0, 0.0, 8192);
    REQUIRE(v.isValidFile());
    v.prefill(4096);

    std::vector<float> readBack;
    readBack.reserve(samples.size());
    std::vector<float> chunk(2048);
    for (int guard = 0; guard < 1000 && readBack.size() < samples.size(); ++guard) {
        const auto got = v.source().read(chunk.data(), chunk.size());
        readBack.insert(readBack.end(), chunk.begin(), chunk.begin() + static_cast<long>(got));
        if (got == 0) {
            if (v.source().atEnd()) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }

    REQUIRE(readBack.size() == samples.size());
    for (std::size_t i = 0; i < samples.size(); ++i) {
        REQUIRE(readBack[i] == samples[i]);
    }

    v.requestStop();
    v.join();
}

TEST_CASE("Voice reports atEnd once the file is exhausted and the buffer drained", "[engine][voice]") {
    std::vector<float> samples(500, 0.3f);
    const auto path = makeTempWavPath("voice_end");
    writeMonoWav(path, samples, 48000);

    Voice v(path, 0, 48000.0, 0.0, 4096);
    v.prefill(500);

    std::vector<float> buf(1000);
    bool reachedEnd = false;
    for (int guard = 0; guard < 500; ++guard) {
        v.source().read(buf.data(), buf.size());
        if (v.source().atEnd()) {
            reachedEnd = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    REQUIRE(reachedEnd);

    v.requestStop();
    v.join();
}
