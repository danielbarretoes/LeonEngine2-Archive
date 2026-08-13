add_executable(Sandbox
    src/SandboxApp.cpp
)

target_include_directories(Sandbox PRIVATE
    "${CMAKE_SOURCE_DIR}/Engine/include"
    "${CMAKE_SOURCE_DIR}/Plugins/RHI/include"
)

target_link_libraries(Sandbox PRIVATE
    Leon::Core
    Leon::OpenGL
)
