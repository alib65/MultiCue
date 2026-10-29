#include <catch2/catch_test_macros.hpp>

#include "mc/engine/DeviceInfo.h"

TEST_CASE("listOutputDevices never throws and returns a usable list", "[engine][device]") {
    // CI runners may have zero real audio devices; the only contract this
    // test can usefully enforce headlessly is "does not crash or throw",
    // plus internal consistency of whatever it does return.
    const auto devices = mc::engine::listOutputDevices();
    for (const auto& d : devices) {
        REQUIRE(d.outputChannels > 0);
    }
}
