#pragma once

#include "core/Base.hpp"
#include "renderer/Buffer.hpp"
#include "renderer/Framebuffer.hpp"
#include "renderer/IBLGenerator.hpp"
#include "renderer/PerspectiveCamera.hpp"
#include "renderer/Shader.hpp"
#include "renderer/VertexArray.hpp"
#include "renderer/PostProcessPipeline.hpp"

#include <glm/glm.hpp>
#include <string>

namespace Leon {

    class FScene;
    struct FDirectionalLightComponent;
    struct FSpotLightComponent;
    struct FSkyboxComponent;
    struct FDirectionalLight;

    // =========================================================================
    // std140-compatible GPU mirror structs — must match UBO layout exactly.
    // =========================================================================

    /** Binding 0 — Camera / Shadow matrices (432 bytes) */
    struct FCameraBufferData {
        glm::mat4 ViewProjection{1.0f};           // 64 bytes
        glm::mat4 LightSpaceMatrices[4]{1.0f};    // 4 * 64 = 256 bytes
        glm::mat4 SpotLightSpaceMatrix{1.0f};     // 64 bytes
        glm::vec4 CameraPosition{0.0f};           // 16 bytes (xyz = position, w = 0)
        glm::vec4 CameraForward{0.0f, 0.0f, -1.0f, 0.0f}; // 16 bytes (xyz = forward dir, w = 0)
        glm::vec4 CascadeSplits{0.0f};            // 16 bytes (x=split0, y=split1, z=split2, w=farClip)
    }; // Total: 432 bytes

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
        FGpuDirectionalLight DirLight;                 // 32 bytes
        FGpuPointLight       PointLights[16];          // 16 * 48 = 768 bytes
        FGpuSpotLight        SpotLights[8];            // 8  * 64 = 512 bytes
        glm::ivec4 LightCounts{0, 0, 0, 0};            // 16 bytes (x = pointCount, y = spotCount)
        glm::vec4  EnvSkyColor{0.18f, 0.44f, 0.88f, 1.2f}; // xyz = sky, w = envIntensity
        glm::vec4  EnvHorizonColor{0.78f, 0.84f, 0.95f, 0.0f};
        glm::vec4  EnvGroundColor{0.22f, 0.24f, 0.28f, 0.0f};
    }; // Total: ~1344 bytes

    // =========================================================================

    /**
     * @brief Owns the full rendering pipeline for a FScene.
     *
     * FScene is data (ECS). FSceneRenderer is behaviour (rendering).
     * Responsible for all framebuffers, UBOs, render passes, and shader orchestration.
     */
    class FSceneRenderer {
    public:
        explicit FSceneRenderer(FScene* InScene);
        ~FSceneRenderer() = default;

        // Non-copyable
        FSceneRenderer(const FSceneRenderer&)            = delete;
        FSceneRenderer& operator=(const FSceneRenderer&) = delete;

        /**
         * @brief Execute all render passes for this frame.
         * Order: Shadow CSM → Spot Shadow → Planar Reflection → Geometry → Skybox → PostProcess
         */
        void Render(const FPerspectiveCamera& InCamera);

        /**
         * @brief Called when the viewport dimensions change. Resizes viewport-dependent FBOs.
         */
        void OnViewportResize(uint32_t InWidth, uint32_t InHeight);

        TRef<FFramebuffer> GetHDRSceneFramebuffer() const { return m_HDRSceneFramebuffer; }
        TRef<FFramebuffer> GetPlanarReflectionFramebuffer() const { return m_PlanarReflectionFramebuffer; }

        void SetDebugMode(int InMode) { m_DebugMode = InMode; }
        int  GetDebugMode() const { return m_DebugMode; }

        FPostProcessSettings& GetPostProcessSettings() { return m_PostProcessSettings; }
        const FPostProcessSettings& GetPostProcessSettings() const { return m_PostProcessSettings; }
        FPostProcessPipeline& GetPostProcessPipeline() { return m_PostProcessPipeline; }

    private:
        // ----- Render Passes -------------------------------------------------
        void RenderCascadedShadowPass(const FPerspectiveCamera& InCamera,
                                      const FDirectionalLightComponent* InDirLightComp,
                                      FCameraBufferData& OutCamData);

        void RenderSpotShadowPass(const FSpotLightComponent* InSpotLightComp,
                                  const glm::vec3& InSpotLightPos,
                                  FCameraBufferData& OutCamData);

        void RenderPlanarReflectionPass(const FPerspectiveCamera& InCamera,
                                        const FSkyboxComponent* InSkybox,
                                        bool bHasDirLight,
                                        const FDirectionalLight& InDirLight);

        void RenderGeometryPass(const FPerspectiveCamera& InCamera,
                                bool bHasDirLight,
                                bool bHasSpotLight,
                                uint32_t InVpWidth,
                                uint32_t InVpHeight);

        void RenderSkyboxPass(const FPerspectiveCamera& InCamera,
                              const FSkyboxComponent* InSkybox,
                              bool bHasDirLight,
                              const FDirectionalLight& InDirLight);

        void RenderPostProcessPass(float InExposure, uint32_t InTargetFBO,
                                   uint32_t InVpWidth, uint32_t InVpHeight);

        void UpdateIBL(const FSkyboxComponent& InSkybox);

        // ----- Members -------------------------------------------------------
        FScene* m_Scene = nullptr;

        uint32_t m_ViewportWidth  = 1280;
        uint32_t m_ViewportHeight = 720;
        int      m_DebugMode      = 0;

        // Tracks the FBO active before Render() was called, restored after PostProcess
        uint32_t m_PreviousFBO = 0;

        // Shadow framebuffers (CSM uses Texture2DArray, Spot uses 2D depth)
        TRef<FFramebuffer> m_CascadeShadowFramebuffer;
        TRef<FFramebuffer> m_SpotShadowFramebuffer;

        // Offscreen targets
        TRef<FFramebuffer> m_PlanarReflectionFramebuffer;
        TRef<FFramebuffer> m_HDRSceneFramebuffer;

        // Uniform buffer objects
        TRef<FUniformBuffer> m_CameraUBO;    // Binding 0
        TRef<FUniformBuffer> m_LightingUBO;  // Binding 1

        // Built-in pipeline shaders
        TRef<FShader> m_ShadowDepthShader;
        TRef<FShader> m_SkyboxShader;
        TRef<FShader> m_PostProcessShader;

        // Built-in geometry
        TRef<FVertexArray> m_SkyboxVA;
        TRef<FVertexArray> m_FullscreenQuadVA;

        // Fallback default 1x1 textures (keeps all texture units valid)
        TRef<FTexture2D> m_DefaultWhiteTexture;
        TRef<FTexture2D> m_DefaultBlackTexture;
        TRef<FTexture2D> m_DefaultFlatNormalTexture;

        // IBL environment
        FIBLEnvironment m_IBLEnvironment;
        bool   m_bUseIBL              = true;
        bool   m_bEnvironmentGenerated = false;
        std::string m_LoadedHDRPath;

        // Post-Processing Pipeline
        FPostProcessPipeline m_PostProcessPipeline;
        FPostProcessSettings m_PostProcessSettings;
    };

} // namespace Leon
