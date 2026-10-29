# Same dual-path story as FindRtAudioPortable.cmake, for libsamplerate.

find_package(SampleRate CONFIG QUIET)

add_library(SampleRatePortable::samplerate INTERFACE IMPORTED)

if(TARGET SampleRate::samplerate)
  target_link_libraries(SampleRatePortable::samplerate INTERFACE SampleRate::samplerate)
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SAMPLERATE_PC REQUIRED IMPORTED_TARGET samplerate)
  target_link_libraries(SampleRatePortable::samplerate INTERFACE PkgConfig::SAMPLERATE_PC)
endif()
