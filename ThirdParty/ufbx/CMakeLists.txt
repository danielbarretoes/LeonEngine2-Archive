add_library(ufbx STATIC
    ufbx.c
    ufbx.h
)

target_include_directories(ufbx PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(ufbx PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
)
