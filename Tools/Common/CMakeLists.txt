add_library(ToolsCommon STATIC
    Source/CommandRegistry.cpp
)

target_include_directories(ToolsCommon PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Include"
)
