#pragma once

#include "Core/Base.hpp"
#include "RHI/FBuffer.hpp"
#include "RHI/FFramebuffer.hpp"
#include "Renderer/FIBLGenerator.hpp"
#include "Renderer/FPerspectiveCamera.hpp"
#include "RHI/FShader.hpp"
#include "RHI/FVertexArray.hpp"
#include "Renderer/FPostProcessPipeline.hpp"
#include "Renderer/FPlanarReflectionTypes.hpp"
#include "Renderer/FShadowTypes.hpp"
#include "Renderer/FShadowMath.hpp"

#include <glm/glm.hpp>
#include <string>
#include <unordered_set>
#include <vector>

namespace Leon {

    class UWorld;
    struct FDirectionalLightComponent;
    struct FSpotLightComponent;
    struct FSkyboxComponent;
    struct FDirectionalLight;

    // =========================================================================
    // std140-compatible GPU mirror structs — must match UBO layout exactly.
    // =========================================================================

    /** Binding 0 — Camera / Shadow matrices & params (464 bytes) */
    struct FCameraBufferData {
        glm::mat4 ViewProjection{1.0f};                   // 64 bytes  (offset 0)
        glm::mat4 LightSpaceMatrices[4]{glm::mat4(1.0f)}; // 256 bytes (offset 64)
        glm::mat4 SpotLightSpaceMatrix{1.0f};             // 64 bytes  (offset 320)
        glm::vec4 CameraPosition{0.0f};                   // 16 bytes  (offset 384)
        glm::vec4 CameraForward{0.0f, 0.0f, -1.0f, 0.0f}; // 16 bytes  (offset 400)
        glm::vec4 CascadeSplits{0.0f};                    // 16 bytes  (offset 416)
        glm::vec4 ShadowParams{0.0010f, 0.0035f, 0.040f,
                               0.25f}; // 16 bytes (offset 432) (x=constBias, y=slopeBias, z=normalBias, w=blendWidth)
        glm::ivec4 ShadowSettings{1, 0, 0,
                                  0}; // 16 bytes (offset 448) (x=filterMode, y=shadowedSpotIndex, z=cascadeCount, w=debug)
    }; // Total: 464 bytes

    /** std140 GPU directional light (PBR — single Intensity, no Phong split) */
    struct FGpuDirectionalLight {
        glm::vec4 Direction{0.0f, -1.0f, 0.0f, 0.0f}; // xyz = dir, w = enabled (1/0)
        glm::vec4 Color{1.0f, 1.0f, 1.0f, 3.0f};      // xyz = color, w = intensity
    }; // 32 bytes

    /** std140 GPU point light — UE4/Filament inverse-square radius model */
    struct FGpuPointLight {
        glm::vec4 Position{0.0f, 0.0f, 0.0f, 0.0f}; // xyz = pos, w = enabled (1/0)
        glm::vec4 Color{1.0f, 1.0f, 1.0f, 8.0f};    // xyz = color, w = intensity
        glm::vec4 Params{10.0f, -1.0f, 0.0f, 0.0f}; // x = radius, y = cubeIndex (-1 = none)
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
        glm::ivec4 LightCounts{0, 0, 0, 0}; // 16 bytes (x = pointCount, y = spotCount, z = shadowedPointCount)
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
         * Order: CSM → Spot Shadow → Planar → Opaque → Skybox → Transparent → SSAO → PostProcess
         */
        void Render(const FPerspectiveCamera& InCamera);
        void RenderScene(const FPerspectiveCamera& InCamera) { Render(InCamera); }

        /**
         * @brief Called when the viewport dimensions change. Resizes viewport-dependent FBOs.
         */
        void OnViewportResize(uint32_t InWidth, uint32_t InHeight);

        TRef<FFramebuffer> GetHDRSceneFramebuffer() const { return HDRSceneFramebuffer; }
        TRef<FFramebuffer> GetPlanarReflectionFramebuffer() const { return PlanarReflectionFramebuffer; }
        TRef<FFramebuffer> GetWallPlanarReflectionFramebuffer() const { return WallPlanarReflectionFramebuffer; }

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
        void EnsureShadowFramebuffers();

        void SetPlanarReflectionEnabled(bool bEnabled) { bEnablePlanarReflection = bEnabled; }
        bool IsPlanarReflectionEnabled() const { return bEnablePlanarReflection; }
        void SetPlanarReflectionQuality(EPlanarReflectionQuality InQuality);
        EPlanarReflectionQuality GetPlanarReflectionQuality() const { return PlanarQuality; }
        void SetPlanarReflectionResolutionScale(float InScale);
        float GetPlanarReflectionResolutionScale() const { return PlanarResolutionScale; }

        /** World plane n·x + Distance = 0. Floor is always captured; a facing wall mirror may use a second FBO. */
        struct FPlanarReflectionPlane {
            glm::vec3 Normal{0.0f, 1.0f, 0.0f};
            float Distance = 0.0f;
        };

        void ClearPlanarReflectionPlanes();
        void AddPlanarReflectionPlane(const glm::vec3& InNormal, float InDistance);

        /**
         * @brief Apply project-level renderer defaults from DefaultEngine.ini.
         * Call before first FBO-heavy work when possible; CascadeResolution is read at construction.
         */
        void ApplyProjectRendererDefaults(uint32_t InShadowMapResolution, bool bInEnablePlanarReflection,
                                          EPlanarReflectionQuality InPlanarQuality = EPlanarReflectionQuality::Epic,
                                          float InPlanarResolutionScale = 0.0f);

        /** Force IBL / HDR environment rebuild on the next frame (e.g. after texture quality change). */
        void InvalidateEnvironment();

    private:
        // ----- Render Passes -------------------------------------------------
        void RenderCascadedShadowPass(const FPerspectiveCamera& InCamera,
                                      const FDirectionalLightComponent* InDirLightComp, FCameraBufferData& OutCamData);

        void RenderSpotShadowPass(const FSpotLightComponent* InSpotLightComp, const glm::vec3& InSpotLightPos,
                                  FCameraBufferData& OutCamData);

        void RenderPointShadowPass(const glm::vec3* InPositions, const float* InRadii, uint32_t InCount);

        void RefreshSkinnedShadowCasterSelection(const glm::vec3& InCameraPos);
        bool IsSkinnedShadowCasterSelected(uint32_t InEntityId) const;

        void DrawShadowCasters(const glm::mat4& InLightSpace, bool bInCullFront, float InPointShadowFarPlane,
                               const glm::vec3& InPointLightPos, bool bInDrawSkinnedCasters);

        void RenderPlanarReflectionPass(const FPerspectiveCamera& InCamera, const FSkyboxComponent* InSkybox,
                                        bool bHasDirLight, const FDirectionalLight& InDirLight);

        void CapturePlanarReflection(const FPerspectiveCamera& InCamera, const FSkyboxComponent* InSkybox,
                                     bool bHasDirLight, const FDirectionalLight& InDirLight,
                                     const FPlanarReflectionPlane& InPlane, FFramebuffer& InTarget,
                                     glm::mat4& OutViewProjection);

        FPlanarReflectionPlane SelectFloorPlane() const;
        bool SelectWallMirrorPlane(const FPerspectiveCamera& InCamera, FPlanarReflectionPlane& OutPlane) const;

        void BindPlanarReflectionUniforms(FShader& InShader, bool bEnabled);

        void RenderOpaqueGeometryPass(const FPerspectiveCamera& InCamera, bool bHasDirLight, bool bHasSpotLight);
        void RenderTransparentGeometryPass(bool bHasDirLight, bool bHasSpotLight);

        void RenderSkyboxPass(const FPerspectiveCamera& InCamera, const FSkyboxComponent* InSkybox, bool bHasDirLight,
                              const FDirectionalLight& InDirLight);

        void RenderPostProcessPass(float InExposure, uint32_t InTargetFBO, uint32_t InVpWidth, uint32_t InVpHeight,
                                   const FPerspectiveCamera& InCamera);

        void UpdateIBL(const FSkyboxComponent& InSkybox);

        void EnsurePlanarFramebuffers();
        uint32_t PlanarCaptureWidth() const;
        uint32_t PlanarCaptureHeight() const;

        // ----- Members -------------------------------------------------------
        UWorld* World = nullptr;

        uint32_t ViewportWidth = 1280;
        uint32_t ViewportHeight = 720;
        int DebugMode = 0;
        bool bWireframeEnabled = false;
        bool bEnablePlanarReflection = true;
        EPlanarReflectionQuality PlanarQuality = EPlanarReflectionQuality::Epic;
        float PlanarResolutionScale = 1.0f;

        // Tracks the FBO active before Render() was called, restored after PostProcess
        uint32_t PreviousFBO = 0;

        // Shadow framebuffers (CSM: 2D array, Spot: 2D, Point: cubemap array)
        TRef<FFramebuffer> CascadeShadowFramebuffer;
        TRef<FFramebuffer> SpotShadowFramebuffer;
        TRef<FFramebuffer> PointShadowFramebuffer;

        // Offscreen targets
        TRef<FFramebuffer> PlanarReflectionFramebuffer;
        TRef<FFramebuffer> WallPlanarReflectionFramebuffer;
        glm::mat4 PlanarViewProjection{1.0f};
        glm::vec3 PlanarPlaneNormal{0.0f, 1.0f, 0.0f};
        float PlanarPlaneDistance = 0.0f;
        glm::mat4 WallPlanarViewProjection{1.0f};
        glm::vec3 WallPlanarPlaneNormal{0.0f, 0.0f, 1.0f};
        float WallPlanarPlaneDistance = 0.0f;
        bool bWallPlanarActive = false;
        std::vector<FPlanarReflectionPlane> PlanarReflectionPlanes;

        TRef<FFramebuffer> HDRSceneFramebuffer;

        // Uniform buffer objects
        TRef<FUniformBuffer> CameraUBO;      // Binding 0
        TRef<FUniformBuffer> LightingUBO;    // Binding 1
        TRef<FUniformBuffer> BonePaletteUBO; // Binding 2 — GPU skinning palette
        TRef<FUniformBuffer> InstanceUBO;    // Binding 3 — opaque instancing matrices

        // Built-in pipeline shaders
        TRef<FShader> ShadowDepthShader;
        TRef<FShader> ShadowDepthSkinnedShader;
        TRef<FShader> SkyboxShader;

        TRef<FVertexArray> SkyboxVA;

        // Fallback default 1x1 textures (keeps all texture units valid)
        TRef<FTexture2D> DefaultWhiteTexture;
        TRef<FTexture2D> DefaultBlackTexture;
        TRef<FTexture2D> DefaultFlatNormalTexture;

        // IBL environment
        FIBLEnvironment IBLEnvironment;
        bool bUseIBL = true;
        bool bEnvironmentGenerated = false;
        std::string LoadedHDRPath;
        int LastTruncatedPointLights = -1;
        int LastTruncatedSpotLights = -1;

        // Post-Processing Pipeline
        FPostProcessPipeline PostProcessPipeline;
        FPostProcessSettings PostProcessSettings;

        // Shadow Settings and Cascade state
        FShadowSettings ShadowSettings;
        std::vector<FShadowCascade> ShadowCascades;
        std::unordered_set<uint32_t> SelectedSkinnedShadowCasters;
        bool bSkinnedShadowSelectionUnlimited = true;
        const FPerspectiveCamera* FrameViewCamera = nullptr;
    };

} // namespace Leon
