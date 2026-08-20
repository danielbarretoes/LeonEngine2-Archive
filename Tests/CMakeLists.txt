if(NOT DEFINED LEON_ENGINE_ROOT)
    get_filename_component(LEON_ENGINE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
endif()

set(LEON_ENGINE_TEST_SOURCES
    Main.cpp
    Math/VectorTBNTests.cpp
    Math/RendererContractTests.cpp
    Math/HammersleyTests.cpp
    Math/HemispherePDFTests.cpp
    IBL/IrradianceConvolutionTests.cpp
    IBL/ReferenceIntegratorTests.cpp
    IBL/SolarHDRRegressionTests.cpp
    IBL/IrradianceContinuityTests.cpp
    IBL/SpecularPrefilterTests.cpp
    PBR/BRDFLUTTests.cpp
    PBR/PBRBrdfTests.cpp
    PBR/EnergyConservationTests.cpp
    HDR/HDRDecodingTests.cpp
    HDR/MipChainTests.cpp
    Cache/SerializationTests.cpp
    Cache/CacheInvalidationTests.cpp
    GPU/CubemapSeamTests.cpp
    Shader/SkyboxEquirectSeamTests.cpp
    GPU/TextureUploadTests.cpp
    IBL/DeterminismTests.cpp
    Shader/PBRShaderFresnelTests.cpp
    Shader/PBRShaderCookTorranceTests.cpp
    Shader/PBRShaderGGXSmithTests.cpp
    Shader/PBRShaderDirectLightsTests.cpp
    Shader/PBRShaderIBLTests.cpp
    Shader/PBRShaderShadowTests.cpp
    Shader/PBRShaderPlanarReflectionTests.cpp
    Shader/PBRShaderPlanarHDRTests.cpp
    Shader/PBRShaderDeterminismTests.cpp
    Shader/PostProcessBloomTests.cpp
    Shader/PostProcessToneMappingTests.cpp
    Shader/PostProcessFXAATests.cpp
    Shader/PostProcessPipelineTests.cpp
    Shader/PostProcessSSAOTests.cpp
    Shader/PBRShaderNormalMappingTests.cpp
    Shader/PBRShaderMaterialTextureTests.cpp
    Shader/PBRShaderEmissiveTests.cpp
    Shader/PBRShaderAlphaTests.cpp
    Shader/PBRShaderUVTransformTests.cpp
    Shader/PBRShaderColorSpaceTests.cpp
    Shader/PBRShaderTangentSpaceTests.cpp
    Shader/ShadowCascadeTests.cpp
    Shader/ShadowPCFTests.cpp
    Shader/ShadowBiasTests.cpp
    Shader/ShadowAtlasTests.cpp
    Shader/ShadowSelectionTests.cpp
    Shader/ShadowPointTests.cpp
    Asset/AssetIDTests.cpp
    Asset/TextureImportTests.cpp
    Asset/HDRAssetTests.cpp
    Asset/MeshImportTests.cpp
    Asset/StaticMeshLODTests.cpp
    Asset/AssetCacheUnloadTests.cpp
    Physics/TriangleMeshCollisionTests.cpp
    Physics/JoltPhysicsSceneTests.cpp
    Asset/SkeletalAnimationTests.cpp
    Asset/AssetValidationTests.cpp
    Gameplay/GameplayFrameworkTests.cpp
    Gameplay/GameplayLifecycleTests.cpp
    Gameplay/ProjectBootTests.cpp
    Gameplay/UIHUDTests.cpp
    Gameplay/GameplayTagTests.cpp
    Gameplay/InputSettingsTests.cpp
    Gameplay/CharacterComponentFrustumTests.cpp
    Gameplay/NetDriverTests.cpp
    Gameplay/EngineGameSeparationTests.cpp
    Gameplay/NamingConventionTests.cpp
    Gameplay/HealthComponentTests.cpp
    Gameplay/InventoryTests.cpp
    Gameplay/ControlSchemaTests.cpp
    Gameplay/OverlapAndDamageTests.cpp
    Gameplay/PhysicsMovementAITests.cpp
    Gameplay/AIPerceptionTests.cpp
    Gameplay/AudioDeviceTests.cpp
    Lightmass/StaticLightingTests.cpp
    Renderer/ViewportResizeTests.cpp
    Renderer/RenderDebugHotkeysTests.cpp
    Renderer/GoldenSceneTests.cpp
    Core/FrameProfilerTests.cpp
    Core/WindowDisplayPolicyTests.cpp
    Performance/ScalingBenchmarkTests.cpp
)

set(LEON_TOURNAMENT_TEST_SOURCES
    Main.cpp
    LeonTournament/LeonTournamentGameplayTests.cpp
    LeonTournament/LifecycleIntegrationTests.cpp
    LeonTournament/PlayabilityIntegrationTests.cpp
    LeonTournament/BotAIIntegrationTests.cpp
    LeonTournament/FirstPersonArenaTests.cpp
    LeonTournament/AnimLabTests.cpp
    LeonTournament/SkeletalImportTests.cpp
    LeonTournament/NamingConventionTests.cpp
    LeonTournament/GraphicsQualityTests.cpp
    LeonTournament/RenderLabTests.cpp
)

set(LEON_TOURNAMENT_RUNTIME_SOURCES
    ${LEON_ENGINE_ROOT}/Projects/LeonTournament/Source/LeonTournament/Private/Game/ALeonTournamentGameMode.cpp
    ${LEON_ENGINE_ROOT}/Projects/LeonTournament/Source/LeonTournament/Private/Game/FLeonTournamentArenaBuilder.cpp
    ${LEON_ENGINE_ROOT}/Projects/LeonTournament/Source/LeonTournament/Private/Game/FLeonTournamentDamageRules.cpp
    ${LEON_ENGINE_ROOT}/Projects/LeonTournament/Source/LeonTournament/Private/Game/FLeonTournamentKillFeed.cpp
    ${LEON_ENGINE_ROOT}/Projects/LeonTournament/Source/LeonTournament/Private/Game/ALeonTournamentGameState.cpp
    ${LEON_ENGINE_ROOT}/Projects/LeonTournament/Source/LeonTournament/Private/Characters/ALeonTournamentCharacter.cpp
    ${LEON_ENGINE_ROOT}/Projects/LeonTournament/Source/LeonTournament/Private/Game/ALeonTournamentPlayerController.cpp
    ${LEON_ENGINE_ROOT}/Projects/LeonTournament/Source/LeonTournament/Private/Game/ALeonTournamentPlayerState.cpp
    ${LEON_ENGINE_ROOT}/Projects/LeonTournament/Source/LeonTournament/Private/Game/ALeonTournamentBotController.cpp
    ${LEON_ENGINE_ROOT}/Projects/LeonTournament/Source/LeonTournament/Private/Combat/ALeonTournamentWeapon.cpp
    ${LEON_ENGINE_ROOT}/Projects/LeonTournament/Source/LeonTournament/Private/Combat/FLeonTournamentWeaponVfx.cpp
    ${LEON_ENGINE_ROOT}/Projects/LeonTournament/Source/LeonTournament/Private/Combat/ALeonTournamentProjectile.cpp
    ${LEON_ENGINE_ROOT}/Projects/LeonTournament/Source/LeonTournament/Private/Combat/ALeonTournamentPickup.cpp
    ${LEON_ENGINE_ROOT}/Projects/LeonTournament/Source/LeonTournament/Private/Game/ALeonTournamentFlag.cpp
    ${LEON_ENGINE_ROOT}/Projects/LeonTournament/Source/LeonTournament/Private/Game/ALeonTournamentFlagBase.cpp
    ${LEON_ENGINE_ROOT}/Projects/LeonTournament/Source/LeonTournament/Private/Game/ALeonTournamentCaptureTheFlagGameMode.cpp
    ${LEON_ENGINE_ROOT}/Projects/LeonTournament/Source/LeonTournament/Private/UI/ALeonTournamentHUD.cpp
    ${LEON_ENGINE_ROOT}/Projects/LeonTournament/Source/LeonTournament/Private/Characters/ALeonTournamentDummy.cpp
    ${LEON_ENGINE_ROOT}/Projects/LeonTournament/Source/LeonTournament/Private/Game/ALeonTournamentAnimLabGameMode.cpp
    ${LEON_ENGINE_ROOT}/Projects/LeonTournament/Source/LeonTournament/Private/Game/ALeonTournamentRenderLabGameMode.cpp
    ${LEON_ENGINE_ROOT}/Projects/LeonTournament/Source/LeonTournament/Private/Game/ALeonTournamentTransitionGameMode.cpp
    ${LEON_ENGINE_ROOT}/Projects/LeonTournament/Source/LeonTournament/Private/Characters/ULeonTournamentAnimInstance.cpp
    ${LEON_ENGINE_ROOT}/Projects/LeonTournament/Source/LeonTournament/Private/Combat/ULeonTournamentCombatComponent.cpp
    ${LEON_ENGINE_ROOT}/Projects/LeonTournament/Source/LeonTournament/Private/Game/ULeonTournamentGameInstance.cpp
    ${LEON_ENGINE_ROOT}/Projects/LeonTournament/Source/LeonTournament/Private/UI/FLeonTournamentUILayout.cpp
    ${LEON_ENGINE_ROOT}/Projects/LeonTournament/Source/LeonTournament/Private/UI/FLeonTournamentCrosshairTextures.cpp
    ${LEON_ENGINE_ROOT}/Projects/LeonTournament/Source/LeonTournament/Private/UI/ULeonTournamentMainMenuWidget.cpp
    ${LEON_ENGINE_ROOT}/Projects/LeonTournament/Source/LeonTournament/Private/UI/ULeonTournamentLobbyWidget.cpp
    ${LEON_ENGINE_ROOT}/Projects/LeonTournament/Source/LeonTournament/Private/UI/ULeonTournamentHUDWidget.cpp
    ${LEON_ENGINE_ROOT}/Projects/LeonTournament/Source/LeonTournament/Private/UI/ULeonTournamentScoreboardWidget.cpp
    ${LEON_ENGINE_ROOT}/Projects/LeonTournament/Source/LeonTournament/Private/UI/ULeonTournamentPauseWidget.cpp
    ${LEON_ENGINE_ROOT}/Projects/LeonTournament/Source/LeonTournament/Private/UI/ULeonTournamentMatchEndWidget.cpp
    ${LEON_ENGINE_ROOT}/Projects/LeonTournament/Source/LeonTournament/Private/UI/ULeonTournamentLoadingOverlayWidget.cpp
    ${LEON_ENGINE_ROOT}/Projects/LeonTournament/Source/LeonTournament/Private/UI/ULeonTournamentRenderLabWidget.cpp
    ${LEON_ENGINE_ROOT}/Projects/LeonTournament/Source/LeonTournament/Private/UI/ALeonTournamentTransitionHUD.cpp
)

function(leon_configure_test_exe TargetName)
    target_include_directories(${TargetName} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${LEON_ENGINE_ROOT}/Plugins/RHI/OpenGL/include
        ${LEON_ENGINE_ROOT}/Plugins/Physics/Jolt/include
        ${LEON_ENGINE_ROOT}/Plugins/Networking/ENet/include
        ${LEON_ENGINE_ROOT}/ThirdParty/glad/include
        ${LEON_ENGINE_ROOT}/ThirdParty/stb
        ${LEON_ENGINE_ROOT}/ThirdParty/doctest
        ${LEON_ENGINE_ROOT}/ThirdParty
    )
    target_link_libraries(${TargetName} PRIVATE
        LeonEngineCore
        LeonEnginePipeline
        LeonEngine_OpenGL
        LeonEngine_Jolt
        LeonEngine_ENet
        doctest
        glad
        glfw
        glm
        stb_image
    )
endfunction()

# Historical binary name kept for mutation scripts / CI that invoke RendererTests.exe.
# This target is engine-only and must not link LeonTournament.
add_executable(RendererTests ${LEON_ENGINE_TEST_SOURCES})
leon_configure_test_exe(RendererTests)

add_executable(LeonTournamentTests ${LEON_TOURNAMENT_TEST_SOURCES} ${LEON_TOURNAMENT_RUNTIME_SOURCES})
leon_configure_test_exe(LeonTournamentTests)
set(LEON_TOURNAMENT_PUBLIC_ROOT "${LEON_ENGINE_ROOT}/Projects/LeonTournament/Source/LeonTournament/Public")
target_include_directories(LeonTournamentTests PRIVATE
    "${LEON_TOURNAMENT_PUBLIC_ROOT}"
    "${LEON_TOURNAMENT_PUBLIC_ROOT}/Types"
    "${LEON_TOURNAMENT_PUBLIC_ROOT}/UI"
    "${LEON_TOURNAMENT_PUBLIC_ROOT}/Game"
    "${LEON_TOURNAMENT_PUBLIC_ROOT}/Characters"
    "${LEON_TOURNAMENT_PUBLIC_ROOT}/Combat"
)

enable_testing()
add_test(NAME EngineTests COMMAND RendererTests WORKING_DIRECTORY ${LEON_ENGINE_ROOT})
add_test(NAME LeonTournamentTests COMMAND LeonTournamentTests WORKING_DIRECTORY ${LEON_ENGINE_ROOT})
add_custom_target(EngineTests DEPENDS RendererTests)
