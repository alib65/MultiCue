#pragma once

#include <sndfile.h>

#include <atomic>
#include <filesystem>
#include <string>
#include <vector>

namespace mc::engine::test {

inline std::string makeTempWavPath(const std::string& tag) {
    static std::atomic<int> counter{0};
    auto dir = std::filesystem::temp_directory_path();
    auto path = dir / ("mc_engine_test_" + tag + "_" + std::to_string(counter.fetch_add(1)) + ".wav");
    return path.string();
}

inline void writeMonoWav(const std::string& path, const std::vector<float>& samples, int sampleRate) {
    SF_INFO info{};
    info.samplerate = sampleRate;
    info.channels = 1;
    info.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;
    SNDFILE* f = sf_open(path.c_str(), SFM_WRITE, &info);
    sf_writef_float(f, samples.data(), static_cast<sf_count_t>(samples.size()));
    sf_close(f);
}

struct WavData {
    std::vector<float> interleaved;
    int channels = 0;
    int sampleRate = 0;
    sf_count_t frames = 0;

    float at(sf_count_t frame, int channel) const {
        return interleaved[static_cast<std::size_t>((frame * channels) + channel)];
    }
};

inline WavData readWav(const std::string& path) {
    SF_INFO info{};
    SNDFILE* f = sf_open(path.c_str(), SFM_READ, &info);
    WavData data;
    if (f == nullptr) {
        return data;
    }
    data.channels = info.channels;
    data.sampleRate = info.samplerate;
    data.frames = info.frames;
    data.interleaved.resize(static_cast<std::size_t>(info.frames * info.channels));
    sf_readf_float(f, data.interleaved.data(), info.frames);
    sf_close(f);
    return data;
}

} // namespace mc::engine::test
