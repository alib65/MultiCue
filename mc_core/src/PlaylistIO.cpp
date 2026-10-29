#include "mc/core/PlaylistIO.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <functional>
#include <vector>

#include "mc/core/AppInfo.h"

namespace mc::core {

namespace {
using json = nlohmann::json;

json trackToJson(const Track& t) {
    return json{
        {"path", t.path},
        {"channel", t.channel},
        {"fileChannelCount", t.fileChannelCount},
        {"fileSampleRate", t.fileSampleRate},
        {"fileDurationSeconds", t.fileDurationSeconds},
        {"outputChannel", t.outputChannel},
        {"gainDb", t.gainDb},
        {"mute", t.mute},
        {"solo", t.solo},
    };
}

Track trackFromJson(const json& j) {
    Track t;
    t.path = j.value("path", std::string());
    t.channel = j.value("channel", 0);
    t.fileChannelCount = j.value("fileChannelCount", 1);
    t.fileSampleRate = j.value("fileSampleRate", 0.0);
    t.fileDurationSeconds = j.value("fileDurationSeconds", 0.0);
    t.outputChannel = j.value("outputChannel", 0);
    t.gainDb = j.value("gainDb", 0.0f);
    t.mute = j.value("mute", false);
    t.solo = j.value("solo", false);
    return t;
}

json pieceToJson(const Piece& p) {
    json tracks = json::array();
    for (const auto& t : p.tracks) {
        tracks.push_back(trackToJson(t));
    }
    return json{
        {"title", p.title},
        {"trimDb", p.trimDb},
        {"tracks", tracks},
    };
}

Piece pieceFromJson(const json& j) {
    Piece p;
    p.title = j.value("title", std::string("Untitled"));
    p.trimDb = j.value("trimDb", 0.0f);
    if (j.contains("tracks") && j["tracks"].is_array()) {
        for (const auto& tj : j["tracks"]) {
            p.tracks.push_back(trackFromJson(tj));
        }
    }
    return p;
}

// Each entry migrates a document *in place* from schema version N to N+1.
// Index 0 migrates v1->v2, etc. Empty today (v1 is the only version); this
// exists so adding v2 later is "append one function", not "invent the
// migration mechanism under deadline" (docs/ARCHITECTURE.md §6.1).
const std::vector<std::function<void(json&)>>& migrationSteps() {
    static const std::vector<std::function<void(json&)>> steps;
    return steps;
}

} // namespace

std::string serializePlaylist(const Playlist& playlist) {
    json piecesJson = json::array();
    for (const auto& p : playlist.pieces) {
        piecesJson.push_back(pieceToJson(p));
    }
    json doc{
        {"format", std::string(kPlaylistFileMagic)},
        {"schemaVersion", kPlaylistSchemaVersion},
        {"settings",
         json{
             {"autoAdvance", playlist.settings.autoAdvance},
             {"gapSeconds", playlist.settings.gapSeconds},
         }},
        {"pieces", piecesJson},
    };
    return doc.dump(2);
}

PlaylistLoadResult deserializePlaylist(const std::string& text) {
    PlaylistLoadResult result;

    json doc;
    try {
        doc = json::parse(text);
    } catch (const json::parse_error& e) {
        result.error = std::string("Invalid JSON: ") + e.what();
        return result;
    }

    if (!doc.is_object() || doc.value("format", std::string()) != std::string(kPlaylistFileMagic)) {
        result.error = "This file is not a MultiCue playlist.";
        return result;
    }

    const int version = doc.value("schemaVersion", 0);
    if (version < 1) {
        result.error = "This file is missing a schema version and cannot be read safely.";
        return result;
    }
    if (version > kPlaylistSchemaVersion) {
        result.error = "This playlist was saved by a newer version of MultiCue.";
        return result;
    }

    const auto& steps = migrationSteps();
    for (int v = version; v < kPlaylistSchemaVersion; ++v) {
        steps[static_cast<std::size_t>(v - 1)](doc);
    }

    if (doc.contains("settings")) {
        const auto& s = doc["settings"];
        result.playlist.settings.autoAdvance = s.value("autoAdvance", true);
        result.playlist.settings.gapSeconds = s.value("gapSeconds", 3.0);
    }
    if (doc.contains("pieces") && doc["pieces"].is_array()) {
        for (const auto& pj : doc["pieces"]) {
            result.playlist.pieces.push_back(pieceFromJson(pj));
        }
    }

    result.success = true;
    return result;
}

bool savePlaylistToFile(const Playlist& playlist, const std::string& path, std::string& error) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        error = "Could not open \"" + path + "\" for writing.";
        return false;
    }
    out << serializePlaylist(playlist);
    if (!out) {
        error = "Write failed for \"" + path + "\".";
        return false;
    }
    return true;
}

PlaylistLoadResult loadPlaylistFromFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        PlaylistLoadResult result;
        result.error = "Could not open \"" + path + "\".";
        return result;
    }
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return deserializePlaylist(text);
}

} // namespace mc::core
