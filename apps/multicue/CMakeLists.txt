find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(APPLE)
  set(MULTICUE_ICON_FILE "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.icns")
  set_source_files_properties(${MULTICUE_ICON_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
endif()

add_executable(multicue
  src/main.cpp
  src/MainWindow.cpp
  src/MainWindow.h
  src/DarkTheme.cpp
  src/DarkTheme.h
  src/LevelMeterWidget.cpp
  src/LevelMeterWidget.h
  resources/icons.qrc
  ${MULTICUE_ICON_FILE}
)

if(APPLE)
  set_target_properties(multicue PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_ICON_FILE icon.icns
  )
endif()

target_link_libraries(multicue PRIVATE mc_core mc_engine mc_video Qt6::Widgets multicue_warnings multicue_sanitizers)
