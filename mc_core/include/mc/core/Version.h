#pragma once

#include <string>

namespace mc::core {

// Human-readable "<name> <version>" string, built at runtime so it can later
// grow a git-describe suffix without touching every call site.
std::string versionString();

} // namespace mc::core
