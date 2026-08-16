#pragma once

#include "Core/Base.hpp"
#include "RHI/FBuffer.hpp"
#include "RHI/FFramebuffer.hpp"
#include "Renderer/FIBLGenerator.hpp"
#include "Renderer/FPerspectiveCamera.hpp"
#include "RHI/FShader.hpp"
#include "RHI/FVertexArray.hpp"
#include "Renderer/FPostProcessPipeline.hpp"
#include "Renderer/FShadowTypes.hpp"
#include "Renderer/FShadowMath.hpp"

#include <glm/glm.hpp>
#include <string>

namespace Leon {

    class UWorld;
    struct UDirectionalLightComponent;
    struct USpotLightComponent;
    struct FSkyboxComponent;
    struct FDirectionalLight;

    // =========================================================================
    // std140-compatible GPU mirror structs — must match UBO layout exactly.
    // =========================================================================

    /** Binding 0 — Camera / Shadow matrices & params (544 bytes) */
    struct FCameraBufferData {
        glm::mat4 ViewProjection{1.0f};                   // 64 bytes  (offset 0)
        glm::mat4 LightSpaceMatrices[4]{glm::mat4(1.0f)}; // 256 bytes (offset 64)
        glm::mat4 SpotLightSpaceMatrix{1.0f};             // 64 bytes  (offset 320)
        glm::vec4 CameraPosition{0.0f};                   // 16 bytes  (offset 384)
        glm::vec4 CameraForward{0.0f, 0.0f, -1.0f, 0.0f}; // 16 bytes  (offset 400)
        glm::vec4 CascadeSplits{0.0f};                    // 16 bytes  (offset 416) (x=s0, y=s1, z=s2, w=s3)
        glm::vec4 CascadeOffsets[4]{glm::vec4(1.0f, 1.0f, 0.0f, 0.0f), glm::vec4(1.0f, 1.0f, 0.0f, 0.0f),
                                    glm::vec4(1.0f, 1.0f, 0.0f, 0.0f),
                                    glm::vec4(1.0f, 1.0f, 0.0f, 0.0f)}; // 64 bytes (offset 432) (xy=scale, zw=offset)
        glm::vec4 ShadowParams{0.0008f, 0.0015f, 0.025f,
                               0.10f}; // 16 bytes (offset 496) (x=constBias, y=slopeBias, z=normalBias, w=blendWidth)
        glm::ivec4 ShadowSettings{
            1, 16, 0, 0}; // 16 bytes  (offset 512) (x=filterMode, y=contactSteps, z=bContactShadows, w=shadowDebugMode)
        glm::vec4 ContactShadowParams{0.35f, 0.05f, 0.0f, 0.0f}; // 16 bytes (offset 528) (x=dist, y=thick, zw=0)
    }; // Total: 544 bytes

    /** std140 GPU directional light (PBR — single Intensity, no Phong split) */
    struct FGpuDirectionalLight {
        glm::vec4 Direction{0.0f, -1.0f, 0.0f, 0.0f}; // xyz = dir, w = enabled (1/0)
        glm::vec4 Color{1.0f, 1.0f, 1.0f, 3.0f};      // xyz = color, w = intensity
    }; // 32 bytes

    /** std140 GPU point light — UE4/Filament inverse-square radius model */
    struct FGpuPointLight {
        glm::vec4 Position{0.0f, 0.0f, 0.0f, 0.0f}; // xyz = pos, w = enabled (1/0)
        glm::vec4 Color{1.0f, 1.0f, 1.0f, 8.0f};    // xyz = color, w = intensity
        glm::vec4 Params{10.0f, 0.0f, 0.0f, 0.0f};  // x = radius, yzw = 0
    }; // 48 bytes

    /** std140 GPU spot light */
    struct FGpuSpotLight {
        glm::vec4 Position{0.0f, 0.0f, 0.0f, 0.0f};   // xyz = pos, w = enabled (1/0)
        glm::vec4 Direction{0.0f, -1.0f, 0.0f, 0.9f}; // xyz = dir, w = cutOff (cos)
        glm::vec4 Color{1.0f, 1.0f, 1.0f, 0.8f};      // xyz = color, w = outerCutOff (cos)
        glm::vec4 Params{10.0f, 10.0f, 0.0f, 0.0f};   // x = radius, y = intensity, zw = 0
    }; // 64 bytes

    /** Binding 1 — Lighting buffer */
    struct FLightingBufferData {
        FGpuDirectionalLight DirLight;                    // 32 bytes
        FGpuPointLight PointLights[16];                   // 16 * 48 = 768 bytes
        FGpuSpotLight SpotLights[8];                      // 8  * 64 = 512 bytes
        glm::ivec4 LightCounts{0, 0, 0, 0};               // 16 bytes (x = pointCount, y = spotCount)
        glm::vec4 EnvSkyColor{0.18f, 0.44f, 0.88f, 1.2f}; // xyz = sky, w = envIntensity
        glm::vec4 EnvHorizonColor{0.78f, 0.84f, 0.95f, 0.0f};
        glm::vec4 EnvGroundColor{0.22f, 0.24f, 0.28f, 0.0f};
    }; // Total: ~1344 bytes

    // =========================================================================

    /**
     * @brief Owns the full rendering pipeline for a UWorld.
     */
    class FWorldRenderer {
    public:
        explicit FWorldRenderer(UWorld* InWorld);
        ~FWorldRenderer() = default;

        // Non-copyable
        FWorldRenderer(const FWorldRenderer&) = delete;
        FWorldRenderer& operator=(const FWorldRenderer&) = delete;

        /**
         * @brief Execute all render passes for this frame.
         * Order: Shadow CSM → Spot Shadow → Planar Reflection → Geometry → Skybox → PostProcess
         */
        void Render(const FPerspectiveCamera& InCamera);
        void RenderScene(const FPerspectiveCamera& InCamera) { Render(InCamera); }

        /**
         * @brief Called when the viewport dimensions change. Resizes viewport-dependent FBOs.
         */
        void OnViewportResize(uint32_t InWidth, uint32_t InHeight);

        TRef<FFramebuffer> GetHDRSceneFramebuffer() const { return HDRSceneFramebuffer; }
        TRef<FFramebuffer> GetPlanarReflectionFramebuffer() const { return PlanarReflectionFramebuffer; }

        void SetDebugMode(int InMode) { DebugMode = InMode; }
        int GetDebugMode() const { return DebugMode; }

        /** @brief Wireframe only during geometry/skybox; always restored before post-process/UI. */
        void SetWireframeEnabled(bool bEnabled) { bWireframeEnabled = bEnabled; }
        bool IsWireframeEnabled() const { return bWireframeEnabled; }

        FPostProcessSettings& GetPostProcessSettings() { return PostProcessSettings; }
        const FPostProcessSettings& GetPostProcessSettings() const { return PostProcessSettings; }
        FPostProcessPipeline& GetPostProcessPipeline() { return PostProcessPipeline; }

        FShadowSettings& GetShadowSettings() { return ShadowSettings; }
        const FShadowSettings& GetShadowSettings() const { return ShadowSettings; }

    private:
        // ----- Render Passes -------------------------------------------------
        void RenderCascadedShadowPass(const FPerspectiveCamera& InCamera,
                                      const UDirectionalLightComponent* InDirLightComp, FCameraBufferData& OutCamData);

        void RenderSpotShadowPass(const USpotLightComponent* InSpotLightComp, const glm::vec3& InSpotLightPos,
                                  FCameraBufferData& OutCamData);

        void RenderPlanarReflectionPass(const FPerspectiveCamera& InCamera, const FSkyboxComponent* InSkybox,
                                        bool bHasDirLight, const FDirectionalLight& InDirLight);

        void RenderGeometryPass(const FPerspectiveCamera& InCamera, bool bHasDirLight, bool bHasSpotLight,
                                uint32_t InVpWidth, uint32_t InVpHeight);

        void RenderSkyboxPass(const FPerspectiveCamera& InCamera, const FSkyboxComponent* InSkybox, bool bHasDirLight,
                              const FDirectionalLight& InDirLight);

        void RenderPostProcessPass(float InExposure, uint32_t InTargetFBO, uint32_t InVpWidth, uint32_t InVpHeight);

        void UpdateIBL(const FSkyboxComponent& InSkybox);

        // ----- Members -------------------------------------------------------
        UWorld* World = nullptr;

        uint32_t ViewportWidth = 1280;
        uint32_t ViewportHeight = 720;
        int DebugMode = 0;
        bool bWireframeEnabled = false;

        // Tracks the FBO active before Render() was called, restored after PostProcess
        uint32_t PreviousFBO = 0;

        // Shadow framebuffers (CSM uses Texture2DArray, Spot uses 2D depth)
        TRef<FFramebuffer> CascadeShadowFramebuffer;
        TRef<FFramebuffer> SpotShadowFramebuffer;

        // Offscreen targets
        TRef<FFramebuffer> PlanarReflectionFramebuffer;
        TRef<FFramebuffer> HDRSceneFramebuffer;

        // Uniform buffer objects
        TRef<FUniformBuffer> CameraUBO;   // Binding 0
        TRef<FUniformBuffer> LightingUBO; // Binding 1

        // Built-in pipeline shaders
        TRef<FShader> ShadowDepthShader;
        TRef<FShader> SkyboxShader;
        TRef<FShader> PostProcessShader;

        // Built-in geometry
        TRef<FVertexArray> SkyboxVA;
        TRef<FVertexArray> FullscreenQuadVA;

        // Fallback default 1x1 textures (keeps all texture units valid)
        TRef<FTexture2D> DefaultWhiteTexture;
        TRef<FTexture2D> DefaultBlackTexture;
        TRef<FTexture2D> DefaultFlatNormalTexture;

        // IBL environment
        FIBLEnvironment IBLEnvironment;
        bool bUseIBL = true;
        bool bEnvironmentGenerated = false;
        std::string LoadedHDRPath;

        // Post-Processing Pipeline
        FPostProcessPipeline PostProcessPipeline;
        FPostProcessSettings PostProcessSettings;

        // Shadow Settings and Cascade state
        FShadowSettings ShadowSettings;
        std::vector<FShadowCascade> ShadowCascades;
    };

    /** @deprecated Prefer FWorldRenderer — kept for transitional includes. */

} // namespace Leon
