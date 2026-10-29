add_library(mc_core STATIC
  src/Version.cpp
)

target_include_directories(mc_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(mc_core PRIVATE multicue_warnings multicue_sanitizers)
target_link_libraries(mc_core PUBLIC multicue_sanitizers)

if(MULTICUE_BUILD_TESTS)
  add_subdirectory(tests)
endif()
