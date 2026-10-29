#include <catch2/catch_test_macros.hpp>

#include "mc/core/AppInfo.h"
#include "mc/core/Version.h"

TEST_CASE("AppInfo constants are non-empty", "[core][appinfo]") {
    REQUIRE_FALSE(mc::core::kAppName.empty());
    REQUIRE_FALSE(mc::core::kAppVersion.empty());
    REQUIRE_FALSE(mc::core::kShowFileMagic.empty());
    REQUIRE(mc::core::kShowFileSchemaVersion >= 1);
}

TEST_CASE("versionString combines name and version", "[core][appinfo]") {
    const auto v = mc::core::versionString();
    REQUIRE(v.find(mc::core::kAppName) != std::string::npos);
    REQUIRE(v.find(mc::core::kAppVersion) != std::string::npos);
}
