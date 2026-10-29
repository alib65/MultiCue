#include "mc/engine/OfflineRenderer.h"

#include <sndfile.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>

#include "mc/engine/FileSampleSource.h"
#include "mc/engine/GainMatrix.h"
#include "mc/engine/PieceMixer.h"

namespace mc::engine {

namespace {
constexpr std::size_t kBlockFrames = 4096;
constexpr double kReleaseTailSeconds = 1.0;
}

OfflineRenderResult renderOffline(const OfflineRenderRequest& request) {
    OfflineRenderResult result;

    if (request.trackPaths.empty()) {
        result.error = "No track files given.";
        return result;
    }

    std::vector<std::unique_ptr<FileSampleSource>> sources;
    sources.reserve(request.trackPaths.size());
    double totalSeconds = 0.0;
    for (const auto& path : request.trackPaths) {
        auto src = std::make_unique<FileSampleSource>(path, 0, request.sampleRate);
        if (!src->isValid()) {
            result.error = "Could not open \"" + path + "\": " + src->lastError();
            return result;
        }
        totalSeconds = std::max(totalSeconds, src->durationSeconds());
        sources.push_back(std::move(src));
    }

    SF_INFO outInfo{};
    outInfo.samplerate = static_cast<int>(request.sampleRate);
    outInfo.channels = static_cast<int>(request.numOutputs);
    outInfo.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;
    SNDFILE* outFile = sf_open(request.outputPath.c_str(), SFM_WRITE, &outInfo);
    if (outFile == nullptr) {
        result.error = "Could not open output \"" + request.outputPath + "\": " + std::string(sf_strerror(nullptr));
        return result;
    }

    PieceMixer mixer(sources.size(), request.numOutputs, request.sampleRate, kBlockFrames);
    mixer.setGainMatrix(GainMatrix::identityDirect(sources.size(), request.numOutputs, request.firstOutput));
    mixer.masterLimiter().setMasterGainDb(request.masterGainDb);
    mixer.masterLimiter().setCeilingDb(request.ceilingDb);
    mixer.masterLimiter().setEnabled(request.limiterEnabled);
    mixer.start();

    std::vector<SampleSource*> trackPtrs;
    trackPtrs.reserve(sources.size());
    for (auto& s : sources) {
        trackPtrs.push_back(s.get());
    }

    std::vector<float> outBlock(kBlockFrames * request.numOutputs);

    const auto totalFrames = static_cast<std::int64_t>(std::llround(totalSeconds * request.sampleRate));
    std::int64_t framesDone = 0;
    bool stopTriggered = false;

    while (true) {
        if (!stopTriggered && framesDone >= totalFrames) {
            mixer.stop();
            stopTriggered = true;
        }
        if (stopTriggered && mixer.isSilent()) {
            break;
        }

        const std::size_t blockFrames = kBlockFrames;
        mixer.processBlock(trackPtrs, outBlock.data(), blockFrames);
        sf_writef_float(outFile, outBlock.data(), static_cast<sf_count_t>(blockFrames));
        framesDone += static_cast<std::int64_t>(blockFrames);

        // Safety valve: never run past totalFrames + kReleaseTailSeconds of
        // tail, even if something keeps isSilent() from ever reporting true.
        if (stopTriggered && framesDone > totalFrames + static_cast<std::int64_t>(kReleaseTailSeconds * request.sampleRate)) {
            break;
        }
    }

    sf_close(outFile);

    result.success = true;
    result.renderedSeconds = static_cast<double>(framesDone) / request.sampleRate;
    return result;
}

} // namespace mc::engine
