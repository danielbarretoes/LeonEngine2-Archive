add_library(LeonEngine_OpenGL STATIC
    src/FOpenGLContext.cpp
    src/FOpenGLRenderAPI.cpp
    src/FOpenGLBuffer.cpp
    src/FOpenGLFramebuffer.cpp
    src/FOpenGLShader.cpp
    src/FOpenGLTexture2D.cpp
    src/FOpenGLTextureCube.cpp
    src/FOpenGLVertexArray.cpp
    src/FOpenGLRenderDriver.cpp
    src/FOpenGLUniformBuffer.cpp
)


add_library(Leon::OpenGL ALIAS LeonEngine_OpenGL)

if(NOT DEFINED LEON_ENGINE_ROOT)
    get_filename_component(LEON_ENGINE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
endif()

target_include_directories(LeonEngine_OpenGL PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${LEON_ENGINE_ROOT}/ThirdParty/glad/include"
    "${LEON_ENGINE_ROOT}/ThirdParty/stb"
)

target_link_libraries(LeonEngine_OpenGL PUBLIC
    Leon::Core
    Leon::RHI
    Leon::Assets
    glad
    glfw
    stb_image
)

if(WIN32)
    target_link_libraries(LeonEngine_OpenGL PUBLIC opengl32)
endif()
