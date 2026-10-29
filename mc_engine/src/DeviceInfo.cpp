#include "mc/engine/DeviceInfo.h"

#include <RtAudio.h>

namespace mc::engine {

std::vector<DeviceInfo> listOutputDevices() {
    std::vector<DeviceInfo> devices;
    try {
        RtAudio rt;
        for (auto id : rt.getDeviceIds()) {
            RtAudio::DeviceInfo info = rt.getDeviceInfo(id);
            if (info.outputChannels == 0) {
                continue;
            }
            DeviceInfo d;
            d.id = id;
            d.name = info.name;
            d.outputChannels = info.outputChannels;
            d.isDefaultOutput = info.isDefaultOutput;
            d.preferredSampleRate = info.preferredSampleRate;
            d.sampleRates.assign(info.sampleRates.begin(), info.sampleRates.end());
            devices.push_back(std::move(d));
        }
    } catch (...) {
        return {};
    }
    return devices;
}

} // namespace mc::engine
