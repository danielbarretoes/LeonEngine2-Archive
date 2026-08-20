add_library(LeonEngine_OpenGL STATIC
    Source/FOpenGLContext.cpp
    Source/FOpenGLRenderAPI.cpp
    Source/FOpenGLBuffer.cpp
    Source/FOpenGLFramebuffer.cpp
    Source/FOpenGLShader.cpp
    Source/FOpenGLTexture2D.cpp
    Source/FOpenGLTextureCube.cpp
    Source/FOpenGLVertexArray.cpp
    Source/FOpenGLRenderDriver.cpp
    Source/FOpenGLUniformBuffer.cpp
)


add_library(Leon::OpenGL ALIAS LeonEngine_OpenGL)

if(NOT DEFINED LEON_ENGINE_ROOT)
    get_filename_component(LEON_ENGINE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
endif()

target_include_directories(LeonEngine_OpenGL PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Include"
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
