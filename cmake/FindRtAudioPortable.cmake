# RtAudio's packaging differs by where it comes from:
#   - vcpkg (used in CI, see vcpkg.json) exports a CMake config target
#     RtAudio::rtaudio.
#   - Homebrew's bottle (used for local macOS development, see README.md)
#   - Debian/Ubuntu's librtaudio-dev
# only ship a pkg-config file. This module tries the CMake config first and
# falls back to pkg-config, exposing a single target name either way:
# RtAudioPortable::rtaudio.

find_package(RtAudio CONFIG QUIET)

add_library(RtAudioPortable::rtaudio INTERFACE IMPORTED)

if(TARGET RtAudio::rtaudio)
  target_link_libraries(RtAudioPortable::rtaudio INTERFACE RtAudio::rtaudio)
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(RTAUDIO_PC REQUIRED IMPORTED_TARGET rtaudio)
  target_link_libraries(RtAudioPortable::rtaudio INTERFACE PkgConfig::RTAUDIO_PC)
endif()
