add_executable(multicue-cli src/main.cpp)

target_link_libraries(multicue-cli PRIVATE mc_core mc_engine multicue_warnings multicue_sanitizers)
