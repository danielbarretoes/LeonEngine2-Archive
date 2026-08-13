add_library(LeonEngine_OpenGL STATIC
    src/OpenGLContext.cpp
    src/OpenGLRenderAPI.cpp
    src/OpenGLBuffer.cpp
    src/OpenGLShader.cpp
    src/OpenGLTexture2D.cpp
    src/OpenGLVertexArray.cpp
    src/OpenGLRenderDriver.cpp
)

add_library(Leon::OpenGL ALIAS LeonEngine_OpenGL)

target_include_directories(LeonEngine_OpenGL PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/Engine/include"
    "${CMAKE_SOURCE_DIR}/ThirdParty/glad/include"
    "${CMAKE_SOURCE_DIR}/ThirdParty/stb"
)

target_link_libraries(LeonEngine_OpenGL PUBLIC
    Leon::Core
    glad
    glfw
    stb_image
)

if(WIN32)
    target_link_libraries(LeonEngine_OpenGL PUBLIC opengl32)
endif()
