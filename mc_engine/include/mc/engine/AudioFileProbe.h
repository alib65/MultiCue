#pragma once

#include <string>

namespace mc::engine {

struct AudioFileInfo {
    bool valid = false;
    std::string error;
    int channels = 0;
    double sampleRate = 0.0;
    double durationSeconds = 0.0;
};

// Reads just the header of a sound file (libsndfile) -- channel count,
// sample rate, duration -- without decoding any audio. Not real-time safe;
// used when adding files to a playlist, not during playback.
AudioFileInfo probeAudioFile(const std::string& path);

} // namespace mc::engine
