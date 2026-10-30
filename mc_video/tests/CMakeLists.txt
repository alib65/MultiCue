add_executable(mc_video_tests
  VideoDecoderTests.cpp
)

target_link_libraries(mc_video_tests PRIVATE mc_video Catch2::Catch2WithMain multicue_warnings multicue_sanitizers)

catch_discover_tests(mc_video_tests)
