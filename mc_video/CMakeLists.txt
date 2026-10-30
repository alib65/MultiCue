include(FindFFmpegPortable)
find_package(Qt6 REQUIRED COMPONENTS OpenGLWidgets)

add_library(mc_video STATIC
  src/VideoDecoder.cpp
  src/VideoSurfaceWidget.cpp
)

target_include_directories(mc_video PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(mc_video
  PUBLIC Qt6::OpenGLWidgets multicue_sanitizers
  PRIVATE FFmpegPortable::ffmpeg multicue_warnings
)

if(MULTICUE_BUILD_TESTS)
  add_subdirectory(tests)
endif()
