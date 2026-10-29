#include <cstdio>
#include <cstring>
#include <string>

#include "mc/core/Version.h"
#include "mc/engine/DeviceInfo.h"

namespace {

void printUsage(const char* argv0) {
    std::printf("%s\n\n", mc::core::versionString().c_str());
    std::printf("Usage: %s <command>\n\n", argv0);
    std::printf("Commands:\n");
    std::printf("  list-devices     List available audio output devices\n");
    std::printf("  version          Print the application version\n");
}

int listDevices() {
    const auto devices = mc::engine::listOutputDevices();
    if (devices.empty()) {
        std::printf("No audio output devices found.\n");
        return 0;
    }
    for (const auto& d : devices) {
        std::printf("[%u]%s %s -- %u output channel(s), preferred %u Hz\n",
                    d.id,
                    d.isDefaultOutput ? " (default)" : "",
                    d.name.c_str(),
                    d.outputChannels,
                    d.preferredSampleRate);
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }
    const std::string command = argv[1];
    if (command == "list-devices") {
        return listDevices();
    }
    if (command == "version") {
        std::printf("%s\n", mc::core::versionString().c_str());
        return 0;
    }
    printUsage(argv[0]);
    return 1;
}
