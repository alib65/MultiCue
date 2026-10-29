# Same dual-path story as FindRtAudioPortable.cmake: vcpkg and Homebrew both
# ship a CMake config (SndFile::sndfile) and a pkg-config module (sndfile)
# for libsndfile; this exposes one target name, SndFilePortable::sndfile,
# regardless of which was found.

find_package(SndFile CONFIG QUIET)

add_library(SndFilePortable::sndfile INTERFACE IMPORTED)

if(TARGET SndFile::sndfile)
  target_link_libraries(SndFilePortable::sndfile INTERFACE SndFile::sndfile)
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SNDFILE_PC REQUIRED IMPORTED_TARGET sndfile)
  target_link_libraries(SndFilePortable::sndfile INTERFACE PkgConfig::SNDFILE_PC)
endif()
