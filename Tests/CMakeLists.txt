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

# Pure Engine test suite (unit / GPU / assets / lifecycle / math / PBR / IBL).
add_executable(RendererTests ${LEON_ENGINE_TEST_SOURCES})
leon_configure_test_exe(RendererTests)

enable_testing()
add_test(NAME EngineTests COMMAND RendererTests WORKING_DIRECTORY ${LEON_ENGINE_ROOT})
add_custom_target(EngineTests DEPENDS RendererTests)
