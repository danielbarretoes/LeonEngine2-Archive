add_executable(Sandbox
    Source/Sandbox/Main.cpp
)

target_include_directories(Sandbox PRIVATE
    "${CMAKE_SOURCE_DIR}/Engine/include"
    "${CMAKE_SOURCE_DIR}/Plugins/RHI/include"
)

target_link_libraries(Sandbox PRIVATE
    Leon::Core
    Leon::OpenGL
)
