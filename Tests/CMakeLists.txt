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
    Asset/AssetIDTests.cpp
    Asset/TextureImportTests.cpp
    Asset/HDRAssetTests.cpp
    Asset/MeshImportTests.cpp
    Asset/SkeletalAnimationTests.cpp
    Asset/AssetValidationTests.cpp
    Gameplay/GameplayFrameworkTests.cpp
    Gameplay/GameplayLifecycleTests.cpp
    Gameplay/ProjectBootTests.cpp
    Gameplay/UIHUDTests.cpp
    Gameplay/InputSettingsTests.cpp
    Gameplay/CharacterComponentFrustumTests.cpp
    Gameplay/NetDriverTests.cpp
    Gameplay/EngineGameSeparationTests.cpp
    Gameplay/NamingConventionTests.cpp
    Gameplay/HealthComponentTests.cpp
    Gameplay/PhysicsMovementAITests.cpp
    Gameplay/AudioDeviceTests.cpp
    Lightmass/StaticLightingTests.cpp
    Renderer/ViewportResizeTests.cpp
    Renderer/GoldenSceneTests.cpp
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
)

set(LEON_TOURNAMENT_RUNTIME_SOURCES
    ${CMAKE_SOURCE_DIR}/Projects/LeonTournament/Source/LeonTournament/Private/ALeonTournamentGameMode.cpp
    ${CMAKE_SOURCE_DIR}/Projects/LeonTournament/Source/LeonTournament/Private/ALeonTournamentGameState.cpp
    ${CMAKE_SOURCE_DIR}/Projects/LeonTournament/Source/LeonTournament/Private/ALeonTournamentCharacter.cpp
    ${CMAKE_SOURCE_DIR}/Projects/LeonTournament/Source/LeonTournament/Private/ALeonTournamentPlayerController.cpp
    ${CMAKE_SOURCE_DIR}/Projects/LeonTournament/Source/LeonTournament/Private/ALeonTournamentPlayerState.cpp
    ${CMAKE_SOURCE_DIR}/Projects/LeonTournament/Source/LeonTournament/Private/ALeonTournamentBotController.cpp
    ${CMAKE_SOURCE_DIR}/Projects/LeonTournament/Source/LeonTournament/Private/ALeonTournamentWeapon.cpp
    ${CMAKE_SOURCE_DIR}/Projects/LeonTournament/Source/LeonTournament/Private/ALeonTournamentHUD.cpp
    ${CMAKE_SOURCE_DIR}/Projects/LeonTournament/Source/LeonTournament/Private/ALeonTournamentDummy.cpp
    ${CMAKE_SOURCE_DIR}/Projects/LeonTournament/Source/LeonTournament/Private/ALeonTournamentAnimLabGameMode.cpp
    ${CMAKE_SOURCE_DIR}/Projects/LeonTournament/Source/LeonTournament/Private/ULeonTournamentAnimInstance.cpp
    ${CMAKE_SOURCE_DIR}/Projects/LeonTournament/Source/LeonTournament/Private/ULeonTournamentCombatComponent.cpp
    ${CMAKE_SOURCE_DIR}/Projects/LeonTournament/Source/LeonTournament/Private/ULeonTournamentGameInstance.cpp
    ${CMAKE_SOURCE_DIR}/Projects/LeonTournament/Source/LeonTournament/Private/ULeonTournamentWidgets.cpp
)

function(leon_configure_test_exe TargetName)
    target_include_directories(${TargetName} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/Plugins/RHI/OpenGL/include
        ${CMAKE_SOURCE_DIR}/Plugins/Physics/Jolt/include
        ${CMAKE_SOURCE_DIR}/Plugins/Networking/ENet/include
        ${CMAKE_SOURCE_DIR}/ThirdParty/glad/include
        ${CMAKE_SOURCE_DIR}/ThirdParty/stb
        ${CMAKE_SOURCE_DIR}/ThirdParty/doctest
        ${CMAKE_SOURCE_DIR}/ThirdParty
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
target_include_directories(LeonTournamentTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Projects/LeonTournament/Source/LeonTournament/Public
)

enable_testing()
add_test(NAME EngineTests COMMAND RendererTests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME LeonTournamentTests COMMAND LeonTournamentTests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_custom_target(EngineTests DEPENDS RendererTests)
