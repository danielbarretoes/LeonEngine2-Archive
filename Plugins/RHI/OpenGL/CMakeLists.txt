add_library(LeonEngine_OpenGL STATIC
    src/OpenGLContext.cpp
    src/OpenGLRenderAPI.cpp
    src/OpenGLBuffer.cpp
    src/OpenGLShader.cpp
    src/OpenGLVertexArray.cpp
    src/OpenGLRenderDriver.cpp
)

add_library(Leon::OpenGL ALIAS LeonEngine_OpenGL)

target_include_directories(LeonEngine_OpenGL PUBLIC
    "${CMAKE_SOURCE_DIR}/Plugins/RHI/include"
    "${CMAKE_SOURCE_DIR}/Engine/include"
    "${CMAKE_SOURCE_DIR}/ThirdParty/glad/include"
)

target_link_libraries(LeonEngine_OpenGL PUBLIC
    Leon::Core
    glad
    glfw
)

if(WIN32)
    target_link_libraries(LeonEngine_OpenGL PUBLIC opengl32)
endif()
