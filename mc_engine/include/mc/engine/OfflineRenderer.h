#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace mc::engine {

// Renders a "piece" (one mono file per track, direct mode: track i -> output
// firstOutput + i) to a sound file, faster than real time, through the same
// PieceMixer/MasterLimiter code the realtime engine uses -- see
// docs/ARCHITECTURE.md §3.7. Used by multicue-cli's `render` subcommand and
// by engine tests (bit-exactness, sample-aligned starts, click-free fades).
struct OfflineRenderRequest {
    std::vector<std::string> trackPaths;
    std::size_t numOutputs = 2;
    std::size_t firstOutput = 0;
    double sampleRate = 48000.0;
    float masterGainDb = 0.0f;
    float ceilingDb = -0.5f;
    bool limiterEnabled = true;
    std::string outputPath;
};

struct OfflineRenderResult {
    bool success = false;
    std::string error;
    double renderedSeconds = 0.0;
};

OfflineRenderResult renderOffline(const OfflineRenderRequest& request);

} // namespace mc::engine
