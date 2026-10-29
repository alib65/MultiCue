#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

namespace mc::engine {

// Opens one RtAudio output stream and plays a sine tone on a single output
// channel. This is the M0 "skeleton" slice of the real-time engine: it
// already follows the audio-thread rules from docs/ARCHITECTURE.md §3.1
// (the callback only touches atomics and a pure, allocation-free generator
// function), but it does not yet have voices, a router, or a cue runtime —
// those arrive in M1.
class TestToneEngine {
public:
    TestToneEngine();
    ~TestToneEngine();

    TestToneEngine(const TestToneEngine&) = delete;
    TestToneEngine& operator=(const TestToneEngine&) = delete;

    // Opens and starts a stream on `deviceId`, sending a sine tone at
    // `frequencyHz` and `amplitudeDb` (dBFS, <= 0) to output channel
    // `targetChannel` (0-based, relative to the device's first channel) out
    // of `numChannels` opened channels. Returns false and sets lastError()
    // on failure; never throws.
    bool start(std::uint32_t deviceId,
               std::uint32_t numChannels,
               std::uint32_t targetChannel,
               double sampleRate,
               double frequencyHz,
               double amplitudeDb);

    void stop();
    bool isRunning() const noexcept;

    // Non-real-time: safe to call from the UI thread only.
    std::string lastError() const;

    // Count of audio-thread underflow/overflow notifications since start().
    // Updated from the RtAudio stream status flag, not from a syscall or
    // logging call, so it is safe to bump from the callback.
    std::uint64_t underrunCount() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace mc::engine
