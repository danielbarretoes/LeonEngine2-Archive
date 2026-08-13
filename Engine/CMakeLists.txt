add_library(LeonEngineCore STATIC
    Core/src/Log.cpp
    Core/src/Window.cpp
    Core/src/Input.cpp
    Core/src/LayerStack.cpp
    Core/src/Application.cpp
    Renderer/src/GraphicsContext.cpp
    Renderer/src/RenderDriver.cpp
    Renderer/src/RenderCommand.cpp
    Renderer/src/Renderer.cpp
    Renderer/src/RenderAPI.cpp
    Renderer/src/Buffer.cpp
    Renderer/src/Shader.cpp
    Renderer/src/VertexArray.cpp
    Renderer/src/PerspectiveCamera.cpp
    Renderer/src/PerspectiveCameraController.cpp
)

add_library(Leon::Core ALIAS LeonEngineCore)

target_include_directories(LeonEngineCore PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(LeonEngineCore PUBLIC
    glfw
    glm::glm
)
