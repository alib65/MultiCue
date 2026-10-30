add_library(mc_dsp STATIC
  src/SphericalHarmonics.cpp
  src/FibonacciSphere.cpp
  src/LayoutPreset.cpp
  src/Vbap.cpp
  src/AmbisonicDecoder.cpp
  src/StereoDownmix.cpp
  src/FumaConversion.cpp
  src/SphericalHeadHrtf.cpp
)

target_include_directories(mc_dsp PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(mc_dsp PRIVATE multicue_warnings multicue_sanitizers)
target_link_libraries(mc_dsp PUBLIC multicue_sanitizers)

if(MULTICUE_BUILD_TESTS)
  add_subdirectory(tests)
endif()
