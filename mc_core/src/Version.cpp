#include "mc/core/Version.h"

#include "mc/core/AppInfo.h"

namespace mc::core {

std::string versionString() {
    return std::string(kAppName) + " " + std::string(kAppVersion);
}

} // namespace mc::core
