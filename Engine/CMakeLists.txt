add_library(LeonEngineCore STATIC
    src/core/Log.cpp
    src/core/Window.cpp
    src/core/Input.cpp
    src/core/LayerStack.cpp
    src/core/Application.cpp
    src/core/PlatformMemory.cpp
    src/renderer/GraphicsContext.cpp
    src/renderer/RenderDriver.cpp
    src/renderer/RenderCommand.cpp
    src/renderer/Renderer.cpp
    src/renderer/RenderAPI.cpp
    src/renderer/Buffer.cpp
    src/renderer/DebugRenderer.cpp
    src/renderer/DebugOverlay.cpp
    src/renderer/MeshPrimitives.cpp
    src/renderer/Shader.cpp
    src/renderer/Texture.cpp
    src/renderer/VertexArray.cpp
    src/renderer/PerspectiveCamera.cpp
    src/renderer/PerspectiveCameraController.cpp
)

add_library(Leon::Core ALIAS LeonEngineCore)

target_include_directories(LeonEngineCore PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/ThirdParty/stb"
)

target_link_libraries(LeonEngineCore PUBLIC
    glfw
    glm::glm
    glad
)

if (WIN32)
    target_link_libraries(LeonEngineCore PRIVATE psapi dxgi)
endif()
