if(NOT DEFINED LEON_ENGINE_ROOT)
    get_filename_component(LEON_ENGINE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
endif()

add_executable(LeonAssetTool
    src/main.cpp
)

target_include_directories(LeonAssetTool PRIVATE
    "${LEON_ENGINE_ROOT}/ThirdParty/stb"
    "${LEON_ENGINE_ROOT}/ThirdParty/ufbx"
)

target_link_libraries(LeonAssetTool PRIVATE
    LeonEngineCore
    LeonEnginePipeline
    glm::glm
)
