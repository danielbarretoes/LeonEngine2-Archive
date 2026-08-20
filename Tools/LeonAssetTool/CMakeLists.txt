if(NOT DEFINED LEON_ENGINE_ROOT)
    get_filename_component(LEON_ENGINE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
endif()

file(GLOB_RECURSE LEON_ASSET_TOOL_SOURCES
    "Source/*.cpp"
)

add_executable(LeonAssetTool
    ${LEON_ASSET_TOOL_SOURCES}
)

target_include_directories(LeonAssetTool PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Include"
    "${LEON_ENGINE_ROOT}/ThirdParty/stb"
    "${LEON_ENGINE_ROOT}/ThirdParty/ufbx"
)

target_link_libraries(LeonAssetTool PRIVATE
    LeonEngineCore
    LeonEnginePipeline
    glm::glm
)
