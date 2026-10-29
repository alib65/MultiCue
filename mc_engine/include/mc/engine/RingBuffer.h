#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <vector>

namespace mc::engine {

// A single-producer/single-consumer lock-free ring buffer of floats.
// write() is called only from the background reader thread; read() is
// called only from the real-time audio thread -- per
// docs/ARCHITECTURE.md §3.2, this is the mechanism that gets samples from
// disk to the audio callback without the callback ever blocking, locking,
// or allocating. Capacity is fixed at construction (rounded up to a power
// of two) so both read() and write() are allocation-free.
class RingBuffer {
public:
    explicit RingBuffer(std::size_t minCapacity) {
        std::size_t cap = 1;
        while (cap < minCapacity) {
            cap <<= 1;
        }
        buffer_.assign(cap, 0.0f);
        mask_ = cap - 1;
    }

    std::size_t capacity() const noexcept { return buffer_.size(); }

    std::size_t availableToRead() const noexcept {
        return writePos_.load(std::memory_order_acquire) - readPos_.load(std::memory_order_relaxed);
    }

    std::size_t availableToWrite() const noexcept {
        return capacity() - (writePos_.load(std::memory_order_relaxed) - readPos_.load(std::memory_order_acquire));
    }

    // Producer-thread only.
    std::size_t write(const float* data, std::size_t count) {
        const std::size_t avail = availableToWrite();
        const std::size_t n = std::min(count, avail);
        std::size_t pos = writePos_.load(std::memory_order_relaxed);
        for (std::size_t i = 0; i < n; ++i) {
            buffer_[(pos + i) & mask_] = data[i];
        }
        writePos_.store(pos + n, std::memory_order_release);
        return n;
    }

    // Consumer-thread only (the audio callback).
    std::size_t read(float* out, std::size_t count) {
        const std::size_t avail = availableToRead();
        const std::size_t n = std::min(count, avail);
        std::size_t pos = readPos_.load(std::memory_order_relaxed);
        for (std::size_t i = 0; i < n; ++i) {
            out[i] = buffer_[(pos + i) & mask_];
        }
        readPos_.store(pos + n, std::memory_order_release);
        return n;
    }

    // Discards all buffered data. Only safe to call when the producer
    // (reader) thread is not concurrently writing -- used when re-seeking a
    // voice, after its reader thread has been stopped/joined.
    void reset() noexcept {
        readPos_.store(writePos_.load(std::memory_order_acquire), std::memory_order_relaxed);
    }

private:
    std::vector<float> buffer_;
    std::size_t mask_ = 0;
    std::atomic<std::size_t> writePos_{0};
    std::atomic<std::size_t> readPos_{0};
};

} // namespace mc::engine
