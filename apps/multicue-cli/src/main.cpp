#include <cstdio>
#include <cstring>
#include <string>

#include <chrono>
#include <thread>

#include "mc/core/Version.h"
#include "mc/engine/DeviceInfo.h"
#include "mc/engine/OfflineRenderer.h"
#include "mc/engine/RealtimeAudioEngine.h"

namespace {

void printUsage(const char* argv0) {
    std::printf("%s\n\n", mc::core::versionString().c_str());
    std::printf("Usage: %s <command>\n\n", argv0);
    std::printf("Commands:\n");
    std::printf("  list-devices                 List available audio output devices\n");
    std::printf("  version                      Print the application version\n");
    std::printf("  render <out.wav> <track...>  Direct-mode mix of mono track files to <out.wav>\n");
    std::printf("                               (track i -> output i). Options:\n");
    std::printf("                                 --rate <Hz>        default 48000\n");
    std::printf("                                 --gain <dB>        master gain, default 0\n");
    std::printf("                                 --ceiling <dB>     limiter ceiling, default -0.5\n");
    std::printf("                                 --no-limiter       disable the safety limiter\n");
    std::printf("  play <device_id> <track...>  Live playback (direct mode) for a few seconds, for\n");
    std::printf("                               manual testing. Options: --seconds N (default 3), --start N\n");
}

int playCommand(int argc, char** argv) {
    if (argc < 4) {
        std::fprintf(stderr, "Usage: play <device_id> <track1> [track2 ...] [--seconds N] [--start N]\n");
        return 1;
    }
    const auto deviceId = static_cast<std::uint32_t>(std::stoul(argv[2]));
    std::vector<std::string> tracks;
    double seconds = 3.0;
    double startAt = 0.0;
    for (int i = 3; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--seconds" && i + 1 < argc) {
            seconds = std::stod(argv[++i]);
        } else if (arg == "--start" && i + 1 < argc) {
            startAt = std::stod(argv[++i]);
        } else {
            tracks.push_back(arg);
        }
    }
    if (tracks.empty()) {
        std::fprintf(stderr, "No track files given.\n");
        return 1;
    }

    mc::engine::RealtimeAudioEngine engine;
    if (!engine.loadPiece(deviceId, tracks, tracks.size(), 0, startAt)) {
        std::fprintf(stderr, "Load failed: %s\n", engine.lastError().c_str());
        return 1;
    }
    std::printf("Loaded %zu track(s), %.2f s duration. Playing for %.1f s...\n", tracks.size(), engine.pieceDurationSeconds(), seconds);
    engine.play();
    std::this_thread::sleep_for(std::chrono::duration<double>(seconds * 0.5));
    std::printf("  position %.2f s, playing=%d\n", engine.positionSeconds(), engine.isPlaying());

    std::printf("Pausing for 0.5 s...\n");
    engine.pause();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    const double pausedAt = engine.positionSeconds();
    std::printf("  position while paused %.2f s, playing=%d\n", pausedAt, engine.isPlaying());

    std::printf("Resuming...\n");
    engine.play();
    std::this_thread::sleep_for(std::chrono::duration<double>(seconds * 0.5));
    std::printf("  position after resume %.2f s (should be close to %.2f s, not jumped)\n", engine.positionSeconds(), pausedAt);

    std::printf("Seeking to %.2f s...\n", startAt + 0.5);
    engine.seek(startAt + 0.5);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    std::printf("  position after seek %.2f s, playing=%d, underruns=%llu\n",
               engine.positionSeconds(),
               engine.isPlaying(),
               static_cast<unsigned long long>(engine.underrunCount()));

    engine.stop();
    std::printf("Stopped. loaded=%d\n", engine.isLoaded());
    return 0;
}

int renderCommand(int argc, char** argv) {
    mc::engine::OfflineRenderRequest req;
    if (argc < 4) {
        std::fprintf(stderr, "Usage: render <out.wav> <track1> [track2 ...] [--rate N] [--gain dB] [--ceiling dB] [--no-limiter]\n");
        return 1;
    }
    req.outputPath = argv[2];
    for (int i = 3; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--rate" && i + 1 < argc) {
            req.sampleRate = std::stod(argv[++i]);
        } else if (arg == "--gain" && i + 1 < argc) {
            req.masterGainDb = std::stof(argv[++i]);
        } else if (arg == "--ceiling" && i + 1 < argc) {
            req.ceilingDb = std::stof(argv[++i]);
        } else if (arg == "--no-limiter") {
            req.limiterEnabled = false;
        } else {
            req.trackPaths.push_back(arg);
        }
    }
    if (req.trackPaths.empty()) {
        std::fprintf(stderr, "No track files given.\n");
        return 1;
    }
    req.numOutputs = req.trackPaths.size();

    const auto result = mc::engine::renderOffline(req);
    if (!result.success) {
        std::fprintf(stderr, "Render failed: %s\n", result.error.c_str());
        return 1;
    }
    std::printf("Rendered %.2f s (%zu track(s) -> %zu output(s)) to %s\n",
               result.renderedSeconds,
               req.trackPaths.size(),
               req.numOutputs,
               req.outputPath.c_str());
    return 0;
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
    if (command == "render") {
        return renderCommand(argc, argv);
    }
    if (command == "play") {
        return playCommand(argc, argv);
    }
    printUsage(argv[0]);
    return 1;
}
