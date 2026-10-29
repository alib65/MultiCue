include(FindRtAudioPortable)
include(FindSndFilePortable)
include(FindSampleRatePortable)

add_library(mc_engine STATIC
  src/DeviceInfo.cpp
  src/SineGenerator.cpp
  src/TestToneEngine.cpp
  src/FileSampleSource.cpp
  src/Envelope.cpp
  src/GainMatrix.cpp
  src/MasterLimiter.cpp
  src/PieceMixer.cpp
  src/OfflineRenderer.cpp
  src/Voice.cpp
  src/RealtimeAudioEngine.cpp
)

target_include_directories(mc_engine PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(mc_engine
  PUBLIC mc_core Threads::Threads multicue_sanitizers
  PRIVATE RtAudioPortable::rtaudio SndFilePortable::sndfile SampleRatePortable::samplerate multicue_warnings
)

if(MULTICUE_BUILD_TESTS)
  add_subdirectory(tests)
endif()
