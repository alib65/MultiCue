include(FindRtAudioPortable)

add_library(mc_engine STATIC
  src/DeviceInfo.cpp
  src/SineGenerator.cpp
  src/TestToneEngine.cpp
)

target_include_directories(mc_engine PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(mc_engine
  PUBLIC mc_core Threads::Threads multicue_sanitizers
  PRIVATE RtAudioPortable::rtaudio multicue_warnings
)

if(MULTICUE_BUILD_TESTS)
  add_subdirectory(tests)
endif()
