# Header-only miniaudio (implementation compiled inside Engine Audio module).
add_library(miniaudio INTERFACE)
add_library(Leon::miniaudio ALIAS miniaudio)
target_include_directories(miniaudio INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
