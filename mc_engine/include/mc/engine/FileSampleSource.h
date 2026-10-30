#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "mc/engine/SampleSource.h"

struct sf_private_tag;
using SNDFILE = sf_private_tag;
struct SRC_STATE_tag;
using SRC_STATE = SRC_STATE_tag;

namespace mc::engine {

// Reads one channel of a sound file (which may itself be mono, or one
// channel of an interleaved multichannel/stereo file), resampled on the fly
// to `targetSampleRate` only when the file's own rate differs -- when they
// match, samples pass through untouched so direct-mode playback is
// bit-exact (docs/ARCHITECTURE.md §9, "direct mode is bit-exact").
//
// Not real-time safe: opens the file and allocates on construction. Used
// directly by the offline render path and by tests; the realtime engine
// wraps one of these per voice behind a ring buffer filled from a background
// reader thread (docs/ARCHITECTURE.md §3.1/§3.4).
class FileSampleSource : public SampleSource {
public:
    FileSampleSource(const std::string& path, std::size_t channel, double targetSampleRate, double startSeconds = 0.0);
    ~FileSampleSource() override;

    FileSampleSource(const FileSampleSource&) = delete;
    FileSampleSource& operator=(const FileSampleSource&) = delete;

    std::size_t read(float* out, std::size_t numFrames) override;
    bool atEnd() const override;

    bool isValid() const noexcept { return file_ != nullptr; }
    std::string lastError() const { return lastError_; }
    double fileSampleRate() const noexcept { return fileSampleRate_; }
    std::size_t fileNumChannels() const noexcept { return fileNumChannels_; }
    double durationSeconds() const noexcept;

private:
    void fillFromFile();

    std::string path_;
    std::size_t channel_ = 0;
    double targetSampleRate_ = 48000.0;
    std::string lastError_;

    SNDFILE* file_ = nullptr;
    double fileSampleRate_ = 0.0;
    std::size_t fileNumChannels_ = 1;
    std::int64_t fileNumFrames_ = 0;
    bool passthrough_ = true;
    bool fileExhausted_ = false;

    SRC_STATE* resampler_ = nullptr;
    double ratio_ = 1.0;

    std::vector<float> interleavedBuf_;
    std::vector<float> monoInput_;
    std::vector<float> resampledOut_;
    std::size_t overflowPos_ = 0; // index into resampledOut_ not yet consumed
    std::size_t overflowLen_ = 0;
};

} // namespace mc::engine
