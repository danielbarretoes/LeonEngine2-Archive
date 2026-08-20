file(GLOB_RECURSE LIGHTMASS_SOURCES
    "Source/*.cpp"
)

add_executable(LightmassTool
    ${LIGHTMASS_SOURCES}
)

target_include_directories(LightmassTool PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Include"
    "${LEON_ENGINE_ROOT}/Tools/Common/Include"
)

target_link_libraries(LightmassTool PRIVATE
    ToolsCommon
    LeonEngineCore
    LeonEnginePipeline
    glm::glm
)
