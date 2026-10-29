add_executable(mc_core_tests
  AppInfoTests.cpp
)

target_link_libraries(mc_core_tests PRIVATE mc_core Catch2::Catch2WithMain multicue_warnings multicue_sanitizers)

catch_discover_tests(mc_core_tests)
