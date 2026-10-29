#include "mc/engine/TestToneEngine.h"

#include <RtAudio.h>

#include <cmath>

#include "mc/engine/SineGenerator.h"

namespace mc::engine {

struct TestToneEngine::Impl {
    RtAudio rtAudio;
    std::atomic<double> frequencyHz{440.0};
    std::atomic<float> amplitude{0.0f};
    std::atomic<std::uint32_t> targetChannel{0};
    std::atomic<std::uint32_t> numChannels{0};
    std::atomic<double> sampleRate{48000.0};
    std::atomic<double> phase{0.0};
    std::atomic<std::uint64_t> underruns{0};
    std::string lastError;

    static int callback(void* outputBuffer,
                         void* /*inputBuffer*/,
                         unsigned int numFrames,
                         double /*streamTime*/,
                         RtAudioStreamStatus status,
                         void* userData) {
        auto* self = static_cast<Impl*>(userData);
        if (status != 0) {
            self->underruns.fetch_add(1, std::memory_order_relaxed);
        }
        double phase = self->phase.load(std::memory_order_relaxed);
        generateSineBlock(static_cast<float*>(outputBuffer),
                          numFrames,
                          self->numChannels.load(std::memory_order_relaxed),
                          self->targetChannel.load(std::memory_order_relaxed),
                          self->sampleRate.load(std::memory_order_relaxed),
                          self->frequencyHz.load(std::memory_order_relaxed),
                          self->amplitude.load(std::memory_order_relaxed),
                          phase);
        self->phase.store(phase, std::memory_order_relaxed);
        return 0;
    }
};

TestToneEngine::TestToneEngine() : impl_(std::make_unique<Impl>()) {}

TestToneEngine::~TestToneEngine() {
    stop();
}

bool TestToneEngine::start(std::uint32_t deviceId,
                           std::uint32_t numChannels,
                           std::uint32_t targetChannel,
                           double sampleRate,
                           double frequencyHz,
                           double amplitudeDb) {
    stop();

    RtAudio::StreamParameters outputParams;
    outputParams.deviceId = deviceId;
    outputParams.nChannels = numChannels;
    outputParams.firstChannel = 0;

    impl_->numChannels.store(numChannels, std::memory_order_relaxed);
    impl_->targetChannel.store(targetChannel, std::memory_order_relaxed);
    impl_->sampleRate.store(sampleRate, std::memory_order_relaxed);
    impl_->frequencyHz.store(frequencyHz, std::memory_order_relaxed);
    impl_->amplitude.store(static_cast<float>(amplitudeDb <= -120.0 ? 0.0 : std::pow(10.0, amplitudeDb / 20.0)),
                           std::memory_order_relaxed);
    impl_->phase.store(0.0, std::memory_order_relaxed);
    impl_->underruns.store(0, std::memory_order_relaxed);

    unsigned int bufferFrames = 256;
    auto err = impl_->rtAudio.openStream(&outputParams,
                                        nullptr,
                                        RTAUDIO_FLOAT32,
                                        static_cast<unsigned int>(sampleRate),
                                        &bufferFrames,
                                        &Impl::callback,
                                        impl_.get());
    if (err != RTAUDIO_NO_ERROR) {
        impl_->lastError = impl_->rtAudio.getErrorText();
        return false;
    }
    err = impl_->rtAudio.startStream();
    if (err != RTAUDIO_NO_ERROR) {
        impl_->lastError = impl_->rtAudio.getErrorText();
        impl_->rtAudio.closeStream();
        return false;
    }
    impl_->lastError.clear();
    return true;
}

void TestToneEngine::stop() {
    if (impl_->rtAudio.isStreamRunning()) {
        impl_->rtAudio.stopStream();
    }
    if (impl_->rtAudio.isStreamOpen()) {
        impl_->rtAudio.closeStream();
    }
}

bool TestToneEngine::isRunning() const noexcept {
    return impl_->rtAudio.isStreamRunning();
}

std::string TestToneEngine::lastError() const {
    return impl_->lastError;
}

std::uint64_t TestToneEngine::underrunCount() const noexcept {
    return impl_->underruns.load(std::memory_order_relaxed);
}

} // namespace mc::engine
