#pragma once

#include <string>

namespace mc::core {

// One mono source of audio: either its own mono file, or one channel of an
// interleaved multichannel/stereo file (`channel` selects which).
struct Track {
    std::string path;
    int channel = 0;
    int fileChannelCount = 1;
    double fileSampleRate = 0.0;
    double fileDurationSeconds = 0.0;

    int outputChannel = 0; // direct-mode routing target (0-based)
    float gainDb = 0.0f;
    bool mute = false;
    bool solo = false;
};

} // namespace mc::core
