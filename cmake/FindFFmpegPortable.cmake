# Same dual-path story as FindRtAudioPortable.cmake / FindSndFilePortable.cmake:
# vcpkg's ffmpeg port ships pkg-config files rather than a modern CMake
# config, same as Homebrew/apt. Three attempts, in order:
#  1. A CMake config (in case a future vcpkg/distro build starts shipping one).
#  2. vcpkg's own find_package(FFMPEG) module (its toolchain intercepts this
#     call and fills legacy FFMPEG_* variables straight from the installed
#     tree -- no pkg-config binary required, which matters on Windows CI
#     where pkgconf isn't on PATH by default).
#  3. pkg-config, for non-vcpkg builds (Homebrew/apt) of the four libav*
#     modules we actually decode with.
# One target name either way: FFmpegPortable::ffmpeg.

find_package(FFmpeg CONFIG QUIET)

add_library(FFmpegPortable::ffmpeg INTERFACE IMPORTED)

if(TARGET FFmpeg::avcodec)
  target_link_libraries(FFmpegPortable::ffmpeg INTERFACE FFmpeg::avformat FFmpeg::avcodec FFmpeg::avutil FFmpeg::swscale)
else()
  find_package(FFMPEG QUIET)
  if(FFMPEG_FOUND)
    target_include_directories(FFmpegPortable::ffmpeg INTERFACE ${FFMPEG_INCLUDE_DIRS})
    target_link_libraries(FFmpegPortable::ffmpeg INTERFACE ${FFMPEG_LIBRARIES})
  else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFMPEG_PC REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil libswscale)
    target_link_libraries(FFmpegPortable::ffmpeg INTERFACE PkgConfig::FFMPEG_PC)
  endif()
endif()
