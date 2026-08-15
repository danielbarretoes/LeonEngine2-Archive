add_library(LeonEngineCore STATIC
    src/core/ConfigFile.cpp
    src/core/Log.cpp
    src/core/Window.cpp
    src/core/Input.cpp
    src/core/LayerStack.cpp
    src/core/Application.cpp
    src/core/UEngine.cpp
    src/core/PlatformMemory.cpp
    src/renderer/GraphicsContext.cpp
    src/renderer/RenderDriver.cpp
    src/renderer/RenderCommand.cpp
    src/renderer/Renderer.cpp
    src/renderer/RenderAPI.cpp
    src/renderer/AssetManager.cpp
    src/renderer/Buffer.cpp
    src/renderer/Framebuffer.cpp
    src/renderer/DebugRenderer.cpp
    src/renderer/DebugOverlay.cpp
    src/renderer/IBLGenerator.cpp
    src/renderer/Material.cpp
    src/renderer/MaterialInstance.cpp
    src/renderer/MeshPrimitives.cpp
    src/renderer/PostProcessPipeline.cpp
    src/renderer/SceneRenderer.cpp
    src/renderer/ShadowMath.cpp
    src/renderer/Shader.cpp
    src/renderer/Texture.cpp
    src/renderer/TextRenderer.cpp
    src/renderer/VertexArray.cpp
    src/renderer/PerspectiveCamera.cpp
    src/renderer/PerspectiveCameraController.cpp
    src/asset/AssetPath.cpp
    src/asset/TextureImporter.cpp
    src/asset/HDRImporter.cpp
    src/asset/MeshImporter.cpp
    src/asset/MaterialImporter.cpp
    src/asset/AssetManifest.cpp
    src/renderer/StaticMesh.cpp
    src/world/UWorld.cpp
    src/world/UGameInstance.cpp
    src/world/MapSerializer.cpp
    src/world/MaterialSerializer.cpp
    src/gameplay/AActor.cpp
    src/gameplay/APawn.cpp
    src/gameplay/ADefaultPawn.cpp
    src/gameplay/APlayerController.cpp
    src/gameplay/APlayerState.cpp
    src/gameplay/AGameStateBase.cpp
    src/gameplay/AGameModeBase.cpp
    src/gameplay/ACameraActor.cpp
    src/gameplay/APlayerCameraManager.cpp
    src/gameplay/UClassRegistry.cpp
)

add_library(Leon::Core ALIAS LeonEngineCore)

target_include_directories(LeonEngineCore PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/ThirdParty/stb"
    "${CMAKE_SOURCE_DIR}/ThirdParty/ufbx"
)

target_link_libraries(LeonEngineCore PUBLIC
    glfw
    glm::glm
    EnTT::EnTT
    ufbx
    stb_image
)


if (WIN32)
    target_link_libraries(LeonEngineCore PRIVATE psapi dxgi)
endif()
