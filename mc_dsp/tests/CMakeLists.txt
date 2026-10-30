add_executable(mc_dsp_tests
  SphericalHarmonicsTests.cpp
  FibonacciSphereTests.cpp
  LayoutPresetTests.cpp
  VbapTests.cpp
  AmbisonicDecoderTests.cpp
  StereoDownmixTests.cpp
  FumaConversionTests.cpp
  SphericalHeadHrtfTests.cpp
)

target_link_libraries(mc_dsp_tests PRIVATE mc_dsp Catch2::Catch2WithMain multicue_warnings multicue_sanitizers)

catch_discover_tests(mc_dsp_tests)
