#pragma once

#include <string>

#include "mc/core/Playlist.h"

namespace mc::core {

// Human-readable JSON, versioned (docs/ARCHITECTURE.md §6.1/§6.2). Not
// real-time safe -- called from the UI thread only.
std::string serializePlaylist(const Playlist& playlist);

struct PlaylistLoadResult {
    bool success = false;
    std::string error;
    Playlist playlist;
};

// Parses `json` and migrates it up to kPlaylistSchemaVersion if it was
// written by an older version of this app. Rejects anything that isn't a
// recognisable playlist document (wrong magic, unparseable JSON, a
// schemaVersion newer than this build understands) rather than guessing.
PlaylistLoadResult deserializePlaylist(const std::string& json);

bool savePlaylistToFile(const Playlist& playlist, const std::string& path, std::string& error);
PlaylistLoadResult loadPlaylistFromFile(const std::string& path);

} // namespace mc::core
