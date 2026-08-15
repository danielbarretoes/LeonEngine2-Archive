add_executable(LeonAssetTool
    src/main.cpp
)

target_include_directories(LeonAssetTool PRIVATE
    "${CMAKE_SOURCE_DIR}/Engine/include"
    "${CMAKE_SOURCE_DIR}/ThirdParty/stb"
    "${CMAKE_SOURCE_DIR}/ThirdParty/ufbx"
)

target_link_libraries(LeonAssetTool PRIVATE
    Leon::Core
    glm::glm
)
