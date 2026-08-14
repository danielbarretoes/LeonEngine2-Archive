#pragma once

#include "core/Base.hpp"
#include "core/Timestep.hpp"
#include "renderer/Buffer.hpp"
#include "renderer/PerspectiveCamera.hpp"

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <string>

#include "renderer/IBLGenerator.hpp"

namespace Leon {

    class FEntity;

    // std140 Camera Buffer (Binding 0)
    struct FCameraBufferData {
        glm::mat4 ViewProjection{1.0f};          // 64 bytes
        glm::mat4 LightSpaceMatrices[4]{1.0f};    // 4 * 64 = 256 bytes
        glm::mat4 SpotLightSpaceMatrix{1.0f};     // 64 bytes
        glm::vec4 CameraPosition{0.0f};          // 16 bytes (xyz = position, w = 0)
        glm::vec4 CascadeSplits{0.0f};           // 16 bytes (x = split0, y = split1, z = split2, w = split3)
    }; // Total: 416 bytes

    // std140 GPU Light Substructures
    struct FGpuDirectionalLight {
        glm::vec4 Direction{0.0f, -1.0f, 0.0f, 0.0f}; // xyz = dir, w = enabled
        glm::vec4 Color{1.0f, 1.0f, 1.0f, 0.1f};      // xyz = color, w = ambientIntensity
        glm::vec4 Intensities{1.0f, 1.0f, 0.0f, 0.0f}; // x = diffuseIntensity, y = specularIntensity, zw = 0
    }; // 48 bytes

    struct FGpuPointLight {
        glm::vec4 Position{0.0f, 0.0f, 0.0f, 0.0f};   // xyz = pos, w = enabled
        glm::vec4 Color{1.0f, 1.0f, 1.0f, 0.05f};     // xyz = color, w = ambientIntensity
        glm::vec4 Attenuation{1.0f, 0.09f, 0.032f, 1.0f}; // x = const, y = lin, z = quad, w = diffuseIntensity
        glm::vec4 Params{1.0f, 0.0f, 0.0f, 0.0f};     // x = specularIntensity, yzw = 0
    }; // 64 bytes

    struct FGpuSpotLight {
        glm::vec4 Position{0.0f, 0.0f, 0.0f, 0.0f};   // xyz = pos, w = enabled
        glm::vec4 Direction{0.0f, -1.0f, 0.0f, 0.9f}; // xyz = dir, w = cutOff (cos)
        glm::vec4 Color{1.0f, 1.0f, 1.0f, 0.8f};      // xyz = color, w = outerCutOff (cos)
        glm::vec4 Attenuation{1.0f, 0.09f, 0.032f, 1.0f}; // x = const, y = lin, z = quad, w = diffuseIntensity
        glm::vec4 Params{0.0f, 1.0f, 0.0f, 0.0f};     // x = ambientIntensity, y = specularIntensity, zw = 0
    }; // 80 bytes

    // std140 Lighting Buffer (Binding 1)
    struct FLightingBufferData {
        FGpuDirectionalLight DirLight;                 // 48 bytes
        FGpuPointLight PointLights[16];                // 16 * 64 = 1024 bytes
        FGpuSpotLight SpotLights[8];                   // 8 * 80 = 640 bytes
        glm::ivec4 LightCounts{0, 0, 0, 0};            // 16 bytes (x = pointCount, y = spotCount)
        glm::vec4 EnvSkyColor{0.18f, 0.44f, 0.88f, 1.2f}; // 16 bytes (xyz = skyColor, w = envIntensity)
        glm::vec4 EnvHorizonColor{0.78f, 0.84f, 0.95f, 0.0f}; // 16 bytes
        glm::vec4 EnvGroundColor{0.22f, 0.24f, 0.28f, 0.0f};  // 16 bytes
    }; // Total: 1776 bytes

    class FScene {
    public:
        FScene();
        ~FScene();

        FEntity CreateEntity(const std::string& InName = std::string());
        void DestroyEntity(FEntity InEntity);

        void OnUpdate(FTimestep InTs);
        void OnRender(const FPerspectiveCamera& InCamera);

        void OnViewportResize(uint32_t InWidth, uint32_t InHeight);

        entt::registry& GetRegistry() { return m_Registry; }
        const entt::registry& GetRegistry() const { return m_Registry; }

        static TRef<FScene> Create();

    private:
        void RenderCascadedShadowPass(const FPerspectiveCamera& InCamera, const FDirectionalLightComponent* InDirLightComp, FCameraBufferData& OutCamData);
        void RenderSpotShadowPass(const FSpotLightComponent* InSpotLightComp, const glm::vec3& InSpotLightPos, FCameraBufferData& OutCamData);
        void RenderPlanarReflectionPass(const FPerspectiveCamera& InCamera, const FSkyboxComponent* InSkybox, bool bHasDirLight, const FDirectionalLight& InDirLight);
        void RenderGeometryPass(const FPerspectiveCamera& InCamera, bool bHasDirLight, bool bHasSpotLight, uint32_t InVpWidth, uint32_t InVpHeight);
        void RenderSkyboxPass(const FPerspectiveCamera& InCamera, const FSkyboxComponent* InSkybox, bool bHasDirLight, const FDirectionalLight& InDirLight);
        void RenderPostProcessPass(float InExposure, uint32_t InTargetFBO, uint32_t InVpWidth, uint32_t InVpHeight);
        void UpdateIBL(const FSkyboxComponent& InSkybox);

        entt::registry m_Registry;
        uint32_t m_ViewportWidth = 1280;
        uint32_t m_ViewportHeight = 720;

        TRef<class FFramebuffer> m_CascadeShadowFramebuffers[3];
        TRef<class FFramebuffer> m_SpotShadowFramebuffer;
        TRef<class FFramebuffer> m_PlanarReflectionFramebuffer;
        TRef<class FFramebuffer> m_HDRSceneFramebuffer;
        TRef<class FUniformBuffer> m_CameraUBO;
        TRef<class FUniformBuffer> m_LightingUBO;
        TRef<class FShader> m_ShadowDepthShader;
        TRef<class FShader> m_SkyboxShader;
        TRef<class FShader> m_PostProcessShader;
        TRef<class FVertexArray> m_SkyboxVA;
        TRef<class FVertexArray> m_FullscreenQuadVA;
        FIBLEnvironment m_IBLEnvironment;
        bool m_bUseIBL = true;
        std::string m_LoadedHDRPath;
        bool m_bEnvironmentGenerated = false;

        friend class FEntity;
    };

} // namespace Leon
