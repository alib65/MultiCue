#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <thread>

#include "mc/engine/FileSampleSource.h"
#include "mc/engine/RingBuffer.h"
#include "mc/engine/SampleSource.h"

namespace mc::engine {

// One realtime-playable mono track: a background thread reads/resamples the
// file (via FileSampleSource) into a RingBuffer, and the audio thread pulls
// from that ring buffer through a SampleSource adapter -- the file I/O and
// resampling the reference's DiskIn/VDiskIn did on the audio server's own
// thread happen here on a thread that is explicitly *not* the audio thread,
// per docs/ARCHITECTURE.md §3.1/§3.4.
class Voice {
public:
    Voice(std::string path, std::size_t channel, double targetSampleRate, double startSeconds, std::size_t ringCapacityFrames);
    ~Voice();

    Voice(const Voice&) = delete;
    Voice& operator=(const Voice&) = delete;

    bool isValidFile() const noexcept { return fileValid_; }
    std::string lastError() const { return lastError_; }
    double durationSeconds() const noexcept { return durationSeconds_; }

    // Non-real-time: blocks the calling thread until the ring buffer holds
    // at least `prefillFrames` samples (or the file is exhausted), so that
    // starting playback right after this call will not immediately
    // underrun. Called before arming a voice, never from the audio thread.
    void prefill(std::size_t prefillFrames);

    // Real-time safe: the audio thread's view of this voice.
    SampleSource& source() noexcept { return *ringSource_; }

    // Signals the background thread to exit; join() waits for it. Call
    // both before destroying a Voice or before re-seeking it.
    void requestStop();
    void join();

    std::size_t underrunCount() const noexcept { return underruns_.load(std::memory_order_relaxed); }

private:
    class RingSource : public SampleSource {
    public:
        RingSource(RingBuffer& ring, std::atomic<bool>& readerFinished, std::atomic<std::size_t>& underruns)
            : ring_(ring), readerFinished_(readerFinished), underruns_(underruns) {}
        std::size_t read(float* out, std::size_t numFrames) override {
            const std::size_t got = ring_.read(out, numFrames);
            if (got < numFrames && !readerFinished_.load(std::memory_order_acquire)) {
                underruns_.fetch_add(1, std::memory_order_relaxed);
            }
            return got;
        }
        bool atEnd() const override {
            return readerFinished_.load(std::memory_order_acquire) && ring_.availableToRead() == 0;
        }

    private:
        RingBuffer& ring_;
        std::atomic<bool>& readerFinished_;
        std::atomic<std::size_t>& underruns_;
    };

    void readerLoop();

    std::unique_ptr<FileSampleSource> file_; // owned and used only by the reader thread
    bool fileValid_ = false;
    std::string lastError_;
    double durationSeconds_ = 0.0;

    RingBuffer ring_;
    std::atomic<bool> readerFinished_{false};
    std::atomic<bool> stopRequested_{false};
    std::atomic<std::size_t> underruns_{0};
    std::thread thread_;
    std::unique_ptr<RingSource> ringSource_;
};

} // namespace mc::engine
