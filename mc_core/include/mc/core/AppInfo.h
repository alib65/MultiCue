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

} // namespace mc::core
