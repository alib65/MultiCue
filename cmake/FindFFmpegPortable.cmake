# Same dual-path story as FindRtAudioPortable.cmake / FindSndFilePortable.cmake:
# vcpkg's ffmpeg port ships pkg-config files rather than a modern CMake
# config, same as Homebrew/apt. This tries a CMake config first (in case a
# future vcpkg/distro build starts shipping one) and falls back to
# pkg-config for the four libav* modules we actually decode with, exposing
# one target name either way: FFmpegPortable::ffmpeg.

find_package(FFmpeg CONFIG QUIET)

add_library(FFmpegPortable::ffmpeg INTERFACE IMPORTED)

if(TARGET FFmpeg::avcodec)
  target_link_libraries(FFmpegPortable::ffmpeg INTERFACE FFmpeg::avformat FFmpeg::avcodec FFmpeg::avutil FFmpeg::swscale)
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(FFMPEG_PC REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil libswscale)
  target_link_libraries(FFmpegPortable::ffmpeg INTERFACE PkgConfig::FFMPEG_PC)
endif()
