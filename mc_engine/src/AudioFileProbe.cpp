#include "mc/engine/AudioFileProbe.h"

#include <sndfile.h>

namespace mc::engine {

AudioFileInfo probeAudioFile(const std::string& path) {
    AudioFileInfo info;
    SF_INFO sfInfo{};
    SNDFILE* f = sf_open(path.c_str(), SFM_READ, &sfInfo);
    if (f == nullptr) {
        info.error = sf_strerror(nullptr);
        return info;
    }
    info.valid = true;
    info.channels = sfInfo.channels;
    info.sampleRate = sfInfo.samplerate;
    info.durationSeconds = (sfInfo.samplerate > 0) ? static_cast<double>(sfInfo.frames) / sfInfo.samplerate : 0.0;
    sf_close(f);
    return info;
}

} // namespace mc::engine
