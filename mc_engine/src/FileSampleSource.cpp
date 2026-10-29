#include "mc/engine/FileSampleSource.h"

#include <sndfile.h>
#include <samplerate.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace mc::engine {

namespace {
constexpr std::size_t kFileReadFrames = 4096;
}

FileSampleSource::FileSampleSource(const std::string& path, std::size_t channel, double targetSampleRate, double startSeconds)
    : path_(path), channel_(channel), targetSampleRate_(targetSampleRate) {
    SF_INFO info{};
    file_ = sf_open(path.c_str(), SFM_READ, &info);
    if (file_ == nullptr) {
        lastError_ = sf_strerror(nullptr);
        return;
    }
    fileSampleRate_ = info.samplerate;
    fileNumChannels_ = static_cast<std::size_t>(info.channels);
    fileNumFrames_ = info.frames;

    if (startSeconds > 0.0) {
        const auto startFrame = static_cast<sf_count_t>(std::llround(startSeconds * fileSampleRate_));
        sf_seek(file_, startFrame, SEEK_SET);
    }

    ratio_ = targetSampleRate_ / fileSampleRate_;
    passthrough_ = std::abs(ratio_ - 1.0) < 1e-9;

    interleavedBuf_.resize(kFileReadFrames * fileNumChannels_);
    monoInput_.resize(kFileReadFrames);

    if (!passthrough_) {
        int error = 0;
        resampler_ = src_new(SRC_SINC_BEST_QUALITY, 1, &error);
        if (resampler_ == nullptr) {
            lastError_ = "libsamplerate: " + std::string(src_strerror(error));
        }
        // Output can run ahead of what the caller asked for in a given
        // read() call because the resampling ratio rarely divides evenly;
        // kFileReadFrames worth of input can produce a little more or less
        // output depending on `ratio_`, so size the overflow buffer with
        // margin and stash any extra for the next read() call.
        resampledOut_.resize(static_cast<std::size_t>(kFileReadFrames * ratio_) + kFileReadFrames);
    }
}

FileSampleSource::~FileSampleSource() {
    if (resampler_ != nullptr) {
        src_delete(resampler_);
    }
    if (file_ != nullptr) {
        sf_close(file_);
    }
}

double FileSampleSource::durationSeconds() const noexcept {
    return (fileSampleRate_ > 0.0) ? static_cast<double>(fileNumFrames_) / fileSampleRate_ : 0.0;
}

bool FileSampleSource::atEnd() const {
    return fileExhausted_ && overflowPos_ >= overflowLen_;
}

void FileSampleSource::fillFromFile() {
    if (file_ == nullptr || fileExhausted_) {
        return;
    }
    const sf_count_t got = sf_readf_float(file_, interleavedBuf_.data(), static_cast<sf_count_t>(kFileReadFrames));
    const auto gotFrames = static_cast<std::size_t>(std::max<sf_count_t>(got, 0));
    for (std::size_t i = 0; i < gotFrames; ++i) {
        const std::size_t idx = (i * fileNumChannels_) + std::min(channel_, fileNumChannels_ - 1);
        monoInput_[i] = interleavedBuf_[idx];
    }
    if (gotFrames < kFileReadFrames) {
        fileExhausted_ = true;
    }

    if (passthrough_) {
        overflowLen_ = gotFrames;
        overflowPos_ = 0;
        std::copy(monoInput_.begin(), monoInput_.begin() + static_cast<long>(gotFrames), resampledOut_.begin());
        return;
    }

    if (resampler_ == nullptr) {
        overflowLen_ = 0;
        overflowPos_ = 0;
        return;
    }

    SRC_DATA data{};
    data.data_in = monoInput_.data();
    data.input_frames = static_cast<long>(gotFrames);
    data.data_out = resampledOut_.data();
    data.output_frames = static_cast<long>(resampledOut_.size());
    data.src_ratio = ratio_;
    data.end_of_input = fileExhausted_ ? 1 : 0;

    const int err = src_process(resampler_, &data);
    overflowLen_ = (err == 0) ? static_cast<std::size_t>(data.output_frames_gen) : 0;
    overflowPos_ = 0;
}

std::size_t FileSampleSource::read(float* out, std::size_t numFrames) {
    if (!isValid()) {
        return 0;
    }
    if (resampledOut_.empty()) {
        resampledOut_.resize(kFileReadFrames);
    }

    std::size_t written = 0;
    while (written < numFrames) {
        if (overflowPos_ >= overflowLen_) {
            if (fileExhausted_) {
                break;
            }
            fillFromFile();
            if (overflowLen_ == 0) {
                break;
            }
        }
        const std::size_t available = overflowLen_ - overflowPos_;
        const std::size_t take = std::min(available, numFrames - written);
        std::copy(resampledOut_.begin() + static_cast<long>(overflowPos_),
                  resampledOut_.begin() + static_cast<long>(overflowPos_ + take),
                  out + written);
        overflowPos_ += take;
        written += take;
    }
    return written;
}

} // namespace mc::engine
