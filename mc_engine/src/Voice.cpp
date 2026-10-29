#include "mc/engine/Voice.h"

#include <algorithm>
#include <chrono>

namespace mc::engine {

namespace {
constexpr std::size_t kReadChunkFrames = 1024;
}

Voice::Voice(std::string path, std::size_t channel, double targetSampleRate, double startSeconds, std::size_t ringCapacityFrames)
    : ring_(ringCapacityFrames) {
    file_ = std::make_unique<FileSampleSource>(path, channel, targetSampleRate, startSeconds);
    fileValid_ = file_->isValid();
    lastError_ = file_->lastError();
    durationSeconds_ = file_->durationSeconds();
    ringSource_ = std::make_unique<RingSource>(ring_, readerFinished_, underruns_);

    if (fileValid_) {
        thread_ = std::thread(&Voice::readerLoop, this);
    } else {
        readerFinished_.store(true, std::memory_order_release);
    }
}

Voice::~Voice() {
    requestStop();
    join();
}

void Voice::readerLoop() {
    std::vector<float> chunk(kReadChunkFrames);
    while (!stopRequested_.load(std::memory_order_acquire)) {
        if (ring_.availableToWrite() < kReadChunkFrames) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }
        const std::size_t got = file_->read(chunk.data(), kReadChunkFrames);
        if (got > 0) {
            std::size_t written = 0;
            while (written < got && !stopRequested_.load(std::memory_order_acquire)) {
                written += ring_.write(chunk.data() + written, got - written);
                if (written < got) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                }
            }
        }
        if (file_->atEnd()) {
            readerFinished_.store(true, std::memory_order_release);
            return;
        }
    }
}

void Voice::prefill(std::size_t prefillFrames) {
    if (!fileValid_) {
        return;
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (ring_.availableToRead() < prefillFrames && !readerFinished_.load(std::memory_order_acquire)) {
        if (std::chrono::steady_clock::now() > deadline) {
            break; // never block the caller forever on a slow/stuck disk
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void Voice::requestStop() {
    stopRequested_.store(true, std::memory_order_release);
}

void Voice::join() {
    if (thread_.joinable()) {
        thread_.join();
    }
}

} // namespace mc::engine
