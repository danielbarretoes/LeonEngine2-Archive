#pragma once

#include "Lightmass/FLightmapBuilder.hpp"
#include "Renderer/FLight.hpp"

#include <glm/glm.hpp>
#include <vector>

namespace Leon {

    struct FBakeVertex {
        glm::vec3 Position{0.0f};
        glm::vec3 Normal{0.0f, 1.0f, 0.0f};
        glm::vec2 LightmapUV{0.0f};
        glm::vec3 Albedo{0.8f};
        float Metallic = 0.0f;
        float Roughness = 0.5f;
        glm::vec3 Emissive{0.0f};
    };

    struct FBakeTriangle {
        uint32_t I0 = 0, I1 = 0, I2 = 0;
        uint32_t ChartIndex = 0;
        bool bCastShadow = true;
    };

    struct FBakeDirectionalLight {
        FDirectionalLight Light;
        /** Static: direct+indirect. Stationary: indirect gather only (direct is dynamic). */
        bool bContributeDirect = true;
    };

    struct FBakePointLight {
        FPointLight Light;
        bool bContributeDirect = true;
    };

    struct FBakeSpotLight {
        FSpotLight Light;
        bool bContributeDirect = true;
    };

    struct FLightBakerScene {
        std::vector<FBakeVertex> Vertices;
        std::vector<FBakeTriangle> Triangles;
        std::vector<FBakeDirectionalLight> DirectionalLights;
        std::vector<FBakePointLight> PointLights;
        std::vector<FBakeSpotLight> SpotLights;
        std::vector<FLightmapChart> Charts;
        uint32_t AtlasWidth = 0;
        uint32_t AtlasHeight = 0;
    };

    struct FLightBakerSettings {
        uint32_t NumIndirectBounces = 2;
        uint32_t SamplesPerTexel = 16;
        float IndirectIntensity = 1.0f;
        bool bAmbientOcclusion = true;
        float AOIntensity = 1.0f;
        float AORadius = 1.0f;
        uint64_t Seed = 0x4C454F4E4C4D4153ull;
    };

    /**
     * @brief CPU path-tracer-style lightmap baker (direct + indirect diffuse + AO).
     * Shared light attenuation conventions with FLight / PBR_Lit.
     */
    class FLightBaker {
    public:
        static void Bake(const FLightBakerScene& InScene, const FLightBakerSettings& InSettings,
                          std::vector<float>& OutRGBA32F);

        static float PointAttenuation(float InDistance, float InRadius);
        static float SpotConeFactor(const glm::vec3& InLightDir, const glm::vec3& InToLight,
                                     float InCutOffDeg, float InOuterCutOffDeg);

        static bool IntersectScene(const FLightBakerScene& InScene, const glm::vec3& InOrigin,
                                    const glm::vec3& InDir, float InMaxT, float& OutT, uint32_t& OutTri,
                                    glm::vec3& OutBary);

        static glm::vec3 CosineSampleHemisphere(const glm::vec3& InNormal, float InU1, float InU2);
    };

} // namespace Leon
