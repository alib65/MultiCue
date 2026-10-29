#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mc::engine {

// One audio output device, as reported by the host audio API (CoreAudio,
// WASAPI, ASIO, JACK or ALSA/PulseAudio, depending on platform).
struct DeviceInfo {
    std::uint32_t id = 0;
    std::string name;
    std::uint32_t outputChannels = 0;
    bool isDefaultOutput = false;
    std::vector<std::uint32_t> sampleRates;
    std::uint32_t preferredSampleRate = 0;
};

// Enumerates the output devices currently visible to the audio host API.
// Never throws: on any host API error it returns an empty list, since a
// device-enumeration failure must not prevent the application from starting.
std::vector<DeviceInfo> listOutputDevices();

} // namespace mc::engine
