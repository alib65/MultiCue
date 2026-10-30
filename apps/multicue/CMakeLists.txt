find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(CMAKE_AUTOMOC ON)

add_executable(multicue
  src/main.cpp
  src/MainWindow.cpp
  src/MainWindow.h
  src/DarkTheme.cpp
  src/DarkTheme.h
  src/LevelMeterWidget.cpp
  src/LevelMeterWidget.h
)

target_link_libraries(multicue PRIVATE mc_core mc_engine Qt6::Widgets multicue_warnings multicue_sanitizers)
