#include <catch2/catch_test_macros.hpp>

#include "mc/core/AutosaveScheduler.h"

using mc::core::AutosaveScheduler;

TEST_CASE("AutosaveScheduler does not fire before any edit", "[core][autosave]") {
    AutosaveScheduler sched(1.0);
    REQUIRE_FALSE(sched.shouldSaveNow(0.0));
    REQUIRE_FALSE(sched.shouldSaveNow(100.0));
}

TEST_CASE("AutosaveScheduler fires once the debounce window has elapsed since an edit", "[core][autosave]") {
    AutosaveScheduler sched(1.0);
    sched.markDirty(10.0);
    REQUIRE_FALSE(sched.shouldSaveNow(10.5));
    REQUIRE(sched.shouldSaveNow(11.0));
}

TEST_CASE("AutosaveScheduler fires only once per edit", "[core][autosave]") {
    AutosaveScheduler sched(1.0);
    sched.markDirty(0.0);
    REQUIRE(sched.shouldSaveNow(2.0));
    REQUIRE_FALSE(sched.shouldSaveNow(3.0));
}

TEST_CASE("A new edit resets the debounce window", "[core][autosave]") {
    AutosaveScheduler sched(1.0);
    sched.markDirty(0.0);
    sched.markDirty(0.9); // resets the clock before the first window elapses
    REQUIRE_FALSE(sched.shouldSaveNow(1.5));
    REQUIRE(sched.shouldSaveNow(2.0));
}

TEST_CASE("isDirty reflects pending-save state", "[core][autosave]") {
    AutosaveScheduler sched(1.0);
    REQUIRE_FALSE(sched.isDirty());
    sched.markDirty(0.0);
    REQUIRE(sched.isDirty());
    sched.shouldSaveNow(2.0);
    REQUIRE_FALSE(sched.isDirty());
}
