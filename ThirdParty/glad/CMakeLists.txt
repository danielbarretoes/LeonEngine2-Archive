add_library(glad STATIC
    src/glad.c
)

add_library(Leon::Glad ALIAS glad)

target_include_directories(glad PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
