file(GLOB_RECURSE PROJECT_TOOL_SOURCES
    "Source/*.cpp"
)

add_executable(ProjectTool
    ${PROJECT_TOOL_SOURCES}
)

target_include_directories(ProjectTool PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Include"
    "${LEON_ENGINE_ROOT}/Tools/Common/Include"
)

target_link_libraries(ProjectTool PRIVATE
    ToolsCommon
    LeonEngineCore
    LeonEnginePipeline
    glm::glm
)
