# LeonEngine2 — Unreal-style Runtime layout (Public/Private folders).
# Single link unit for now: cross-module includes (Core↔RHI) match the previous monolith.
# Folder modules (Core, RHI, Assets, …) remain the include/organization contract.

set(LEON_RUNTIME_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/Source/Runtime")

set(LEON_ENGINE_SOURCES
    # Core
    Source/Runtime/Core/Private/FApplication.cpp
    Source/Runtime/Core/Private/FConfigFile.cpp
    Source/Runtime/Core/Private/FInput.cpp
    Source/Runtime/Core/Private/FLayerStack.cpp
    Source/Runtime/Core/Private/FLog.cpp
    Source/Runtime/Core/Private/FPlatformMemory.cpp
    Source/Runtime/Core/Private/FProjectDescriptor.cpp
    Source/Runtime/Core/Private/FProjectPaths.cpp
    Source/Runtime/Core/Private/FWindow.cpp
    # RHI
    Source/Runtime/RHI/Private/IGraphicsContext.cpp
    Source/Runtime/RHI/Private/IRenderAPI.cpp
    Source/Runtime/RHI/Private/IRenderDriver.cpp
    Source/Runtime/RHI/Private/FRenderCommand.cpp
    Source/Runtime/RHI/Private/FBuffer.cpp
    Source/Runtime/RHI/Private/FFramebuffer.cpp
    Source/Runtime/RHI/Private/FShader.cpp
    Source/Runtime/RHI/Private/FTexture.cpp
    Source/Runtime/RHI/Private/FVertexArray.cpp
    Source/Runtime/RHI/Private/FRenderer.cpp
    # Assets
    Source/Runtime/Assets/Private/UAssetManager.cpp
    Source/Runtime/Assets/Private/UStaticMesh.cpp
    Source/Runtime/Assets/Private/USkeleton.cpp
    Source/Runtime/Assets/Private/USkeletalMesh.cpp
    Source/Runtime/Assets/Private/UAnimSequence.cpp
    Source/Runtime/Assets/Private/UBlendSpace.cpp
    Source/Runtime/Assets/Private/FAnimRuntime.cpp
    Source/Runtime/Assets/Private/FAssetManifest.cpp
    Source/Runtime/Assets/Private/FAssetPath.cpp
    # Native .lhdr / .ltex loaders are runtime (OpenGL + IBL). Source import stays callable from Core
    # because stb_image is already a Core dependency; Pipeline keeps FBX (ufbx) + Lightmass.
    Source/Runtime/Assets/Private/FHDRImporter.cpp
    Source/Runtime/Assets/Private/FTextureImporter.cpp
    Source/Runtime/Assets/Private/FLightmapAsset.cpp
    # Renderer
    Source/Runtime/Renderer/Private/FWorldRenderer.cpp
    Source/Runtime/Renderer/Private/FMaterial.cpp
    Source/Runtime/Renderer/Private/FMaterialInstance.cpp
    Source/Runtime/Renderer/Private/FMeshPrimitives.cpp
    Source/Runtime/Renderer/Private/FPerspectiveCamera.cpp
    Source/Runtime/Renderer/Private/FPostProcessPipeline.cpp
    Source/Runtime/Renderer/Private/FIBLGenerator.cpp
    Source/Runtime/Renderer/Private/FDebugOverlay.cpp
    Source/Runtime/Renderer/Private/FDebugRenderer.cpp
    Source/Runtime/Renderer/Private/FParticleRenderer.cpp
    Source/Runtime/Renderer/Private/FTextRenderer.cpp
    Source/Runtime/Renderer/Private/FShadowMath.cpp
    # UMG
    Source/Runtime/UMG/Private/UWidget.cpp
    Source/Runtime/UMG/Private/UPanelWidget.cpp
    Source/Runtime/UMG/Private/UCanvasPanel.cpp
    Source/Runtime/UMG/Private/UTextBlock.cpp
    Source/Runtime/UMG/Private/UEditableText.cpp
    Source/Runtime/UMG/Private/UButton.cpp
    Source/Runtime/UMG/Private/UUserWidget.cpp
    Source/Runtime/UMG/Private/UImage.cpp
    Source/Runtime/UMG/Private/FUIRenderer.cpp
    # Audio
    Source/Runtime/Audio/Private/FAudioDevice.cpp
    Source/Runtime/Audio/Private/USoundWave.cpp
    Source/Runtime/Audio/Private/UAudioComponent.cpp
    # Engine + Gameplay
    Source/Runtime/Engine/Private/UEngine.cpp
    Source/Runtime/Engine/Private/UWorld.cpp
    Source/Runtime/Engine/Private/UGameInstance.cpp
    Source/Runtime/Engine/Private/FMapSerializer.cpp
    Source/Runtime/Engine/Private/FMaterialSerializer.cpp
    Source/Runtime/Engine/Private/UNetDriver.cpp
    Source/Runtime/Engine/Private/FLoopbackNetDriver.cpp
    Source/Runtime/Engine/Private/UIpNetDriver.cpp
    Source/Runtime/Gameplay/Private/AActor.cpp
    Source/Runtime/Gameplay/Private/ACameraActor.cpp
    Source/Runtime/Gameplay/Private/ACharacter.cpp
    Source/Runtime/Gameplay/Private/ADefaultPawn.cpp
    Source/Runtime/Gameplay/Private/AGameModeBase.cpp
    Source/Runtime/Gameplay/Private/AGameStateBase.cpp
    Source/Runtime/Gameplay/Private/AHUD.cpp
    Source/Runtime/Gameplay/Private/APawn.cpp
    Source/Runtime/Gameplay/Private/APlayerCameraManager.cpp
    Source/Runtime/Gameplay/Private/APlayerController.cpp
    Source/Runtime/Gameplay/Private/APlayerState.cpp
    Source/Runtime/Gameplay/Private/APlayerStart.cpp
    Source/Runtime/Gameplay/Private/AWorldSettings.cpp
    Source/Runtime/Gameplay/Private/UGameplayStatics.cpp
    Source/Runtime/Gameplay/Private/FOnScreenDebugMessage.cpp
    Source/Runtime/Gameplay/Private/UActorComponent.cpp
    Source/Runtime/Gameplay/Private/USceneComponent.cpp
    Source/Runtime/Gameplay/Private/UHealthComponent.cpp
    Source/Runtime/Gameplay/Private/UCombatComponent.cpp
    Source/Runtime/Gameplay/Private/UClassRegistry.cpp
    Source/Runtime/Gameplay/Private/UAnimInstance.cpp
    Source/Runtime/Gameplay/Private/USkeletalMeshComponent.cpp
    Source/Runtime/Gameplay/Private/FAnimStateMachine.cpp
    Source/Runtime/Physics/Private/FPhysicsModule.cpp
    Source/Runtime/Physics/Private/FSimplePhysicsScene.cpp
    Source/Runtime/Physics/Private/FCollisionQuery.cpp
    Source/Runtime/Gameplay/Private/UPrimitiveComponent.cpp
    Source/Runtime/Gameplay/Private/UNavMovementComponent.cpp
    Source/Runtime/Gameplay/Private/UCharacterMovementComponent.cpp
    Source/Runtime/Gameplay/Private/UProjectileMovementComponent.cpp
    Source/Runtime/Gameplay/Private/AProjectile.cpp
    Source/Runtime/Gameplay/Private/USpringArmComponent.cpp
    Source/Runtime/Gameplay/Private/UParticleComponent.cpp
    Source/Runtime/Gameplay/Private/AController.cpp
    Source/Runtime/Gameplay/Private/AAIController.cpp
    Source/Runtime/Gameplay/Private/ABlockingVolume.cpp
    Source/Runtime/Gameplay/Private/APhysicsVolume.cpp
    Source/Runtime/Gameplay/Private/ANavMeshBoundsVolume.cpp
    Source/Runtime/Gameplay/Private/FGameplayDebugger.cpp
    Source/Runtime/AI/Private/UBlackboardData.cpp
    Source/Runtime/AI/Private/UBlackboardComponent.cpp
    Source/Runtime/AI/Private/UBehaviorTree.cpp
    Source/Runtime/AI/Private/UBehaviorTreeComponent.cpp
    Source/Runtime/AI/Private/UNavigationSystem.cpp
    Source/Runtime/AI/Private/UPathFollowingComponent.cpp
    Source/Runtime/Assets/Private/UPhysicsAsset.cpp
)

add_library(LeonEngineCore STATIC ${LEON_ENGINE_SOURCES})
add_library(Leon::Core ALIAS LeonEngineCore)
add_library(Leon::Engine ALIAS LeonEngineCore)
add_library(Leon::RHI ALIAS LeonEngineCore)
add_library(Leon::Assets ALIAS LeonEngineCore)
add_library(Leon::Renderer ALIAS LeonEngineCore)
add_library(Leon::UMG ALIAS LeonEngineCore)
add_library(Leon::Gameplay ALIAS LeonEngineCore)

target_include_directories(LeonEngineCore
    PUBLIC
        "${LEON_RUNTIME_ROOT}/Core/Public"
        "${LEON_RUNTIME_ROOT}/RHI/Public"
        "${LEON_RUNTIME_ROOT}/Assets/Public"
        "${LEON_RUNTIME_ROOT}/Renderer/Public"
        "${LEON_RUNTIME_ROOT}/UMG/Public"
        "${LEON_RUNTIME_ROOT}/Engine/Public"
        "${LEON_RUNTIME_ROOT}/Gameplay/Public"
        "${LEON_RUNTIME_ROOT}/Physics/Public"
        "${LEON_RUNTIME_ROOT}/AI/Public"
        "${LEON_RUNTIME_ROOT}/Audio/Public"
        "${CMAKE_SOURCE_DIR}/ThirdParty/stb"
)

target_compile_features(LeonEngineCore PUBLIC cxx_std_20)

target_link_libraries(LeonEngineCore
    PUBLIC
        glfw
        glm::glm
        EnTT::EnTT
        stb_image
        miniaudio
)

if(WIN32)
    target_link_libraries(LeonEngineCore PRIVATE psapi dxgi ws2_32)
endif()

set(LEON_PIPELINE_SOURCES
    Source/Runtime/Assets/Private/FMaterialImporter.cpp
    Source/Runtime/Assets/Private/FMeshImporter.cpp
    Source/Runtime/Assets/Private/FSkeletalImporter.cpp
    Source/Runtime/Assets/Private/FLightmapUV.cpp
    Source/Runtime/Lightmass/Private/FLightmapBuilder.cpp
    Source/Runtime/Lightmass/Private/FLightBaker.cpp
    Source/Runtime/Lightmass/Private/FLightmass.cpp
)

add_library(LeonEnginePipeline STATIC ${LEON_PIPELINE_SOURCES})
add_library(Leon::Pipeline ALIAS LeonEnginePipeline)

target_include_directories(LeonEnginePipeline
    PUBLIC
        "${LEON_RUNTIME_ROOT}/Lightmass/Public"
        "${LEON_RUNTIME_ROOT}/Assets/Public"
        "${CMAKE_SOURCE_DIR}/ThirdParty/ufbx"
)

target_compile_features(LeonEnginePipeline PUBLIC cxx_std_20)

target_link_libraries(LeonEnginePipeline
    PUBLIC
        LeonEngineCore
        ufbx
)

