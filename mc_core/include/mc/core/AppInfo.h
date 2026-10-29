#pragma once

#include <string_view>

namespace mc::core {

// Single source of truth for the application's identity. Every window
// title, show-file magic string, settings path and log header reads from
// here so the "MultiCue" working name can be changed in one place.
inline constexpr std::string_view kAppName = "MultiCue";
inline constexpr std::string_view kAppOrganization = "MultiCue Project";
inline constexpr std::string_view kAppVersion = "0.1.0";
inline constexpr std::string_view kShowFileMagic = "MultiCue-Show";
inline constexpr int kShowFileSchemaVersion = 1;

// Playlist mode (M2) persists as its own document type -- a restricted,
// cue-free shape -- ahead of the full Show file (M5's cue lists). See
// docs/ARCHITECTURE.md §6.2.
inline constexpr std::string_view kPlaylistFileMagic = "MultiCue-Playlist";
inline constexpr int kPlaylistSchemaVersion = 1;

} // namespace mc::core
