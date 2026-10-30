#include "mc/engine/RealtimeAudioEngine.h"

#include <RtAudio.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>

#include "mc/engine/GainMatrix.h"
#include "mc/engine/PieceMixer.h"
#include "mc/engine/Voice.h"

namespace mc::engine {

namespace {
constexpr std::size_t kBufferFrames = 256;
constexpr double kPrefillSeconds = 0.5;
constexpr double kRingSeconds = 2.0;
constexpr double kTransportFadeSeconds = 0.03;
}

// Thread-safety note: loadPiece()/unload() stop the RtAudio stream (which
// blocks until the callback thread is no longer executing) before replacing
// `mixer`/`sourcePtrs`/`voices`, and only start the stream again once the
// new state is fully built -- so there is never a callback in flight while
// those non-atomic members are mutated.
//
// Two independent fades are used once a piece is loaded and running:
//  - Pause/resume use `transportGain`, a float the callback ramps towards
//    `fadeTarget` (0 = pausing out, 1 = resuming in) over ~30 ms, applied
//    as a post-mix multiply. Pausing keeps pulling from the voices' ring
//    buffers for that ~30 ms (a few hundred samples of position drift,
//    inaudible and not worth avoiding) and then stops pulling entirely, so
//    resume continues from exactly where consumption stopped.
//  - Stop uses PieceMixer's own per-track release envelope (the same one
//    used for a fresh start's attack), so a Stop cue fades out tracks the
//    same way the engine already guarantees elsewhere; `stopRequested`
//    tells the callback to keep pulling until PieceMixer::isSilent(), at
//    which point `stoppedFlag` is raised for the owning thread to tear down.
struct RealtimeAudioEngine::Impl {
    RtAudio rtAudio;
    bool streamOpen = false;
    std::uint32_t deviceId = 0;
    std::size_t numOutputs = 0;
    double sampleRate = 48000.0;

    std::vector<std::unique_ptr<Voice>> voices;
    std::vector<SampleSource*> sourcePtrs;
    std::unique_ptr<PieceMixer> mixer;

    std::vector<std::string> trackPaths;
    std::vector<int> outputChannels;

    std::atomic<bool> wantActive{false};  // user wants audio advancing (playing or resuming)
    std::atomic<bool> fadeTarget{false};  // transportGain ramps towards this (pause/resume)
    std::atomic<bool> stopRequested{false};
    std::atomic<bool> stoppedFlag{false}; // callback -> owner: release fade finished
    std::atomic<std::uint64_t> framesPlayed{0};
    std::atomic<bool> deviceLost{false};

    // Audio-thread-only (never touched elsewhere while a stream is open).
    float transportGain = 0.0f;
    bool pulling = false;

    static int callback(void* outputBuffer,
                         void* /*inputBuffer*/,
                         unsigned int numFrames,
                         double /*streamTime*/,
                         RtAudioStreamStatus /*status*/,
                         void* userData) {
        auto* self = static_cast<Impl*>(userData);
        auto* out = static_cast<float*>(outputBuffer);
        const std::size_t totalSamples = static_cast<std::size_t>(numFrames) * self->numOutputs;

        const bool wantActive = self->wantActive.load(std::memory_order_acquire);
        const bool stopReq = self->stopRequested.load(std::memory_order_acquire);

        if (!wantActive && !self->pulling && !stopReq) {
            std::fill(out, out + totalSamples, 0.0f);
            return 0;
        }

        self->mixer->processBlock(self->sourcePtrs, out, numFrames);
        self->framesPlayed.fetch_add(numFrames, std::memory_order_relaxed);

        if (stopReq) {
            self->pulling = !self->mixer->isSilent();
            if (!self->pulling) {
                self->stoppedFlag.store(true, std::memory_order_release);
            }
        } else {
            const bool target = self->fadeTarget.load(std::memory_order_acquire);
            const float tgt = target ? 1.0f : 0.0f;
            const float coeff = static_cast<float>(std::exp(-1.0 / (kTransportFadeSeconds * self->sampleRate)));
            for (unsigned int f = 0; f < numFrames; ++f) {
                self->transportGain += (tgt - self->transportGain) * (1.0f - coeff);
                for (std::size_t ch = 0; ch < self->numOutputs; ++ch) {
                    out[(f * self->numOutputs) + ch] *= self->transportGain;
                }
            }
            self->pulling = wantActive || (self->transportGain > 1e-4f);
        }
        return 0;
    }

    void closeStream() {
        wantActive.store(false, std::memory_order_release);
        if (streamOpen) {
            if (rtAudio.isStreamRunning()) {
                rtAudio.stopStream();
            }
            if (rtAudio.isStreamOpen()) {
                rtAudio.closeStream();
            }
            streamOpen = false;
        }
        pulling = false;
        transportGain = 0.0f;
    }

    bool openStream(std::uint32_t device, std::size_t outs, double rate, std::string& error) {
        RtAudio::StreamParameters params;
        params.deviceId = device;
        params.nChannels = static_cast<unsigned int>(outs);
        params.firstChannel = 0;

        deviceLost.store(false, std::memory_order_relaxed);
        rtAudio.setErrorCallback([this](RtAudioErrorType type, const std::string& /*errorText*/) {
            // Real-time-safe: no allocation, just an atomic flag. Device
            // loss must never crash the app (docs/ARCHITECTURE.md §3.8);
            // the owning (non-RT) thread surfaces a message via
            // deviceWasLost() and getErrorText() if it wants more detail.
            if (type == RTAUDIO_DEVICE_DISCONNECT) {
                deviceLost.store(true, std::memory_order_release);
            }
        });

        unsigned int bufferFrames = kBufferFrames;
        auto err = rtAudio.openStream(&params, nullptr, RTAUDIO_FLOAT32, static_cast<unsigned int>(rate), &bufferFrames, &Impl::callback, this);
        if (err != RTAUDIO_NO_ERROR) {
            error = rtAudio.getErrorText();
            return false;
        }
        err = rtAudio.startStream();
        if (err != RTAUDIO_NO_ERROR) {
            error = rtAudio.getErrorText();
            rtAudio.closeStream();
            return false;
        }
        streamOpen = true;
        deviceId = device;
        numOutputs = outs;
        sampleRate = rate;
        return true;
    }

    // Only safe to call while no stream is open (no callback in flight).
    void teardownVoices() {
        for (auto& v : voices) {
            v->requestStop();
        }
        for (auto& v : voices) {
            v->join();
        }
        voices.clear();
        sourcePtrs.clear();
        mixer.reset();
    }
};

RealtimeAudioEngine::RealtimeAudioEngine() : impl_(std::make_unique<Impl>()) {}

RealtimeAudioEngine::~RealtimeAudioEngine() {
    unload();
}

bool RealtimeAudioEngine::loadPiece(std::uint32_t deviceId,
                                   const std::vector<std::string>& trackPaths,
                                   std::size_t numOutputs,
                                   const std::vector<int>& outputChannels,
                                   double startSeconds) {
    unload();

    if (trackPaths.empty() || numOutputs == 0) {
        lastError_ = "No tracks or no outputs given.";
        return false;
    }
    if (!outputChannels.empty() && outputChannels.size() != trackPaths.size()) {
        lastError_ = "outputChannels size must match trackPaths size.";
        return false;
    }

    RtAudio::DeviceInfo info;
    try {
        info = impl_->rtAudio.getDeviceInfo(deviceId);
    } catch (...) {
        lastError_ = "Could not query the selected device.";
        return false;
    }
    const double sampleRate = (info.preferredSampleRate > 0) ? static_cast<double>(info.preferredSampleRate) : 48000.0;

    const std::size_t ringCapacity = static_cast<std::size_t>(kRingSeconds * sampleRate);
    const std::size_t prefillFrames = static_cast<std::size_t>(kPrefillSeconds * sampleRate);

    std::vector<std::unique_ptr<Voice>> voices;
    voices.reserve(trackPaths.size());
    double maxDuration = 0.0;
    for (const auto& path : trackPaths) {
        auto v = std::make_unique<Voice>(path, 0, sampleRate, startSeconds, ringCapacity);
        if (!v->isValidFile()) {
            lastError_ = "Could not open \"" + path + "\": " + v->lastError();
            return false;
        }
        maxDuration = std::max(maxDuration, v->durationSeconds());
        voices.push_back(std::move(v));
    }
    for (auto& v : voices) {
        v->prefill(prefillFrames);
    }

    auto mixer = std::make_unique<PieceMixer>(voices.size(), numOutputs, sampleRate, kBufferFrames * 4);
    GainMatrix matrix(voices.size(), numOutputs);
    for (std::size_t i = 0; i < voices.size(); ++i) {
        const int out = outputChannels.empty() ? static_cast<int>(i) : outputChannels[i];
        if (out >= 0 && static_cast<std::size_t>(out) < numOutputs) {
            matrix.setGain(i, static_cast<std::size_t>(out), 1.0f);
        }
    }
    mixer->setGainMatrix(matrix);

    std::vector<SampleSource*> sourcePtrs;
    sourcePtrs.reserve(voices.size());
    for (auto& v : voices) {
        sourcePtrs.push_back(&v->source());
    }

    impl_->voices = std::move(voices);
    impl_->sourcePtrs = std::move(sourcePtrs);
    impl_->mixer = std::move(mixer);
    impl_->trackPaths = trackPaths;
    impl_->outputChannels.resize(trackPaths.size());
    for (std::size_t i = 0; i < trackPaths.size(); ++i) {
        impl_->outputChannels[i] = outputChannels.empty() ? static_cast<int>(i) : outputChannels[i];
    }
    impl_->framesPlayed.store(static_cast<std::uint64_t>(startSeconds * sampleRate), std::memory_order_relaxed);
    impl_->stopRequested.store(false, std::memory_order_relaxed);
    impl_->stoppedFlag.store(false, std::memory_order_relaxed);
    impl_->fadeTarget.store(false, std::memory_order_relaxed);
    everStarted_ = false;
    pieceDurationSeconds_ = maxDuration;

    std::string openError;
    if (!impl_->openStream(deviceId, numOutputs, sampleRate, openError)) {
        lastError_ = "Could not open the audio stream: " + openError;
        impl_->teardownVoices();
        return false;
    }

    lastError_.clear();
    return true;
}

void RealtimeAudioEngine::unload() {
    impl_->closeStream();
    impl_->teardownVoices();
    pieceDurationSeconds_ = 0.0;
    everStarted_ = false;
}

void RealtimeAudioEngine::play() {
    if (!impl_->mixer) {
        return;
    }
    if (!everStarted_) {
        impl_->mixer->start();
        everStarted_ = true;
    }
    impl_->fadeTarget.store(true, std::memory_order_release);
    impl_->wantActive.store(true, std::memory_order_release);
}

void RealtimeAudioEngine::pause() {
    impl_->fadeTarget.store(false, std::memory_order_release);
    impl_->wantActive.store(false, std::memory_order_release);
}

void RealtimeAudioEngine::requestStop() {
    if (!impl_->mixer || !everStarted_) {
        impl_->stoppedFlag.store(true, std::memory_order_relaxed); // nothing to fade
        return;
    }
    // Make sure the transport gain is settled at 1 so the audible fade the
    // user hears is entirely PieceMixer's own release envelope, not a
    // leftover pause-fade fighting it.
    impl_->fadeTarget.store(true, std::memory_order_release);
    impl_->wantActive.store(true, std::memory_order_release);
    impl_->mixer->stop();
    impl_->stoppedFlag.store(false, std::memory_order_relaxed);
    impl_->stopRequested.store(true, std::memory_order_release);
}

bool RealtimeAudioEngine::isStopFadeComplete() const noexcept {
    return impl_->stoppedFlag.load(std::memory_order_acquire);
}

void RealtimeAudioEngine::stop() {
    requestStop();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    while (!isStopFadeComplete() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    unload();
}

void RealtimeAudioEngine::seek(double seconds) {
    if (impl_->trackPaths.empty()) {
        return;
    }
    const bool wasActive = impl_->wantActive.load(std::memory_order_acquire);
    const auto paths = impl_->trackPaths;
    const auto outputChannels = impl_->outputChannels;
    const auto numOutputs = impl_->numOutputs;
    const auto deviceId = impl_->deviceId;

    if (!loadPiece(deviceId, paths, numOutputs, outputChannels, std::max(0.0, seconds))) {
        return;
    }
    if (wasActive) {
        play();
    }
}

bool RealtimeAudioEngine::isLoaded() const noexcept {
    return impl_->mixer != nullptr;
}

bool RealtimeAudioEngine::isPlaying() const noexcept {
    return impl_->wantActive.load(std::memory_order_acquire);
}

double RealtimeAudioEngine::positionSeconds() const noexcept {
    if (impl_->sampleRate <= 0.0) {
        return 0.0;
    }
    return static_cast<double>(impl_->framesPlayed.load(std::memory_order_relaxed)) / impl_->sampleRate;
}

bool RealtimeAudioEngine::deviceWasLost() const noexcept {
    return impl_->deviceLost.load(std::memory_order_acquire);
}

std::uint64_t RealtimeAudioEngine::underrunCount() const noexcept {
    std::uint64_t total = 0;
    for (const auto& v : impl_->voices) {
        total += v->underrunCount();
    }
    return total;
}

float RealtimeAudioEngine::trackPeak(std::size_t track) const noexcept {
    return impl_->mixer ? impl_->mixer->trackPeak(track) : 0.0f;
}
float RealtimeAudioEngine::trackRms(std::size_t track) const noexcept {
    return impl_->mixer ? impl_->mixer->trackRms(track) : 0.0f;
}
float RealtimeAudioEngine::outputPeak(std::size_t output) const noexcept {
    return impl_->mixer ? impl_->mixer->outputPeak(output) : 0.0f;
}
float RealtimeAudioEngine::outputRms(std::size_t output) const noexcept {
    return impl_->mixer ? impl_->mixer->outputRms(output) : 0.0f;
}
std::size_t RealtimeAudioEngine::numOutputs() const noexcept {
    return impl_->numOutputs;
}

void RealtimeAudioEngine::setTrackGainDb(std::size_t track, float db) {
    if (impl_->mixer) {
        impl_->mixer->setTrackGainDb(track, db);
    }
}

void RealtimeAudioEngine::setTrackMuted(std::size_t track, bool muted) {
    if (impl_->mixer) {
        impl_->mixer->setTrackMuted(track, muted);
    }
}

void RealtimeAudioEngine::setTrackSolo(std::size_t track, bool solo) {
    if (impl_->mixer) {
        impl_->mixer->setTrackSolo(track, solo);
    }
}

void RealtimeAudioEngine::setTrackOutputChannel(std::size_t track, int outputChannel) {
    if (!impl_->mixer || track >= impl_->outputChannels.size()) {
        return;
    }
    impl_->outputChannels[track] = outputChannel;
    GainMatrix matrix(impl_->outputChannels.size(), impl_->numOutputs);
    for (std::size_t i = 0; i < impl_->outputChannels.size(); ++i) {
        const int out = impl_->outputChannels[i];
        if (out >= 0 && static_cast<std::size_t>(out) < impl_->numOutputs) {
            matrix.setGain(i, static_cast<std::size_t>(out), 1.0f);
        }
    }
    impl_->mixer->setGainMatrix(matrix);
}

void RealtimeAudioEngine::setMasterGainDb(float db) {
    if (impl_->mixer) {
        impl_->mixer->masterLimiter().setMasterGainDb(db);
    }
}

void RealtimeAudioEngine::setCeilingDb(float db) {
    if (impl_->mixer) {
        impl_->mixer->masterLimiter().setCeilingDb(db);
    }
}

void RealtimeAudioEngine::setLimiterEnabled(bool enabled) {
    if (impl_->mixer) {
        impl_->mixer->masterLimiter().setEnabled(enabled);
    }
}

} // namespace mc::engine
