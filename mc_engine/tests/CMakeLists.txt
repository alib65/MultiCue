add_executable(mc_engine_tests
  SineGeneratorTests.cpp
  DeviceInfoTests.cpp
  GainMatrixTests.cpp
  EnvelopeTests.cpp
  MasterLimiterTests.cpp
  FileSampleSourceTests.cpp
  PieceMixerTests.cpp
  OfflineRendererTests.cpp
  VoiceTests.cpp
)

target_link_libraries(mc_engine_tests PRIVATE mc_engine Catch2::Catch2WithMain multicue_warnings multicue_sanitizers)

catch_discover_tests(mc_engine_tests)
