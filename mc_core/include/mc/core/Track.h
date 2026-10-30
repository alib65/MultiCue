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

    // Spatial position, meaningful only when the piece's source format is
    // Multi (speaker feed): azimuth 0 = front, +90 = left, -90 = right,
    // 180 = back; elevation 0 = ear height, +90 = straight up. Added in
    // schema v2 alongside the listening-format renderer
    // (docs/ARCHITECTURE.md §0/§4). Ignored (and not meaningful) for AmbiX/
    // FuMa tracks, whose spatial meaning comes from their ACN channel
    // index instead -- see SourceFormat below.
    double azimuthDeg = 0.0;
    double elevationDeg = 0.0;
    bool lfe = false; // low-frequency/subwoofer channel: not placed in space
};

} // namespace mc::core
