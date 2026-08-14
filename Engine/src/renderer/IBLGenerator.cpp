#include "renderer/IBLGenerator.hpp"
#include "core/Log.hpp"
#include "renderer/AssetManager.hpp"
#include "renderer/Renderer.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace Leon {

    static constexpr float PI = 3.14159265358979323846f;

    static float RadicalInverse_VdC(uint32_t bits) {
        bits = (bits << 16u) | (bits >> 16u);
        bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
        bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
        bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
        bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
        return static_cast<float>(bits) * 2.3283064365386963e-10f; // / 0x100000000
    }

    static glm::vec2 Hammersley(uint32_t i, uint32_t N) {
        return glm::vec2(static_cast<float>(i) / static_cast<float>(N), RadicalInverse_VdC(i));
    }

    static glm::vec3 ImportanceSampleGGX(glm::vec2 Xi, glm::vec3 N, float roughness) {
        float a = roughness * roughness;
        float phi = 2.0f * PI * Xi.x;
        float cosTheta = std::sqrt((1.0f - Xi.y) / (1.0f + (a * a - 1.0f) * Xi.y));
        float sinTheta = std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));

        // From spherical coordinates to cartesian coordinates (tangent space)
        glm::vec3 H;
        H.x = std::cos(phi) * sinTheta;
        H.y = std::sin(phi) * sinTheta;
        H.z = cosTheta;

        // From tangent space to world space
        glm::vec3 up = (std::abs(N.z) < 0.999f) ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
        glm::vec3 tangent = glm::normalize(glm::cross(up, N));
        glm::vec3 bitangent = glm::cross(N, tangent);

        glm::vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
        return glm::normalize(sampleVec);
    }

    static float GeometrySchlickGGX_IBL(float NdotV, float roughness) {
        float k = (roughness * roughness) / 2.0f;
        float nom = NdotV;
        float denom = NdotV * (1.0f - k) + k;
        return nom / std::max(denom, 0.0000001f);
    }

    static float GeometrySmith_IBL(glm::vec3 N, glm::vec3 V, glm::vec3 L, float roughness) {
        float NdotV = std::max(glm::dot(N, V), 0.0f);
        float NdotL = std::max(glm::dot(N, L), 0.0f);
        float ggx2 = GeometrySchlickGGX_IBL(NdotV, roughness);
        float ggx1 = GeometrySchlickGGX_IBL(NdotL, roughness);
        return ggx1 * ggx2;
    }

    static glm::vec2 IntegrateBRDF(float NdotV, float roughness) {
        glm::vec3 V;
        V.x = std::sqrt(std::max(0.0f, 1.0f - NdotV * NdotV));
        V.y = 0.0f;
        V.z = NdotV;

        float A = 0.0f;
        float B = 0.0f;

        glm::vec3 N = glm::vec3(0.0f, 0.0f, 1.0f);

        const uint32_t SAMPLE_COUNT = 512u;
        for (uint32_t i = 0u; i < SAMPLE_COUNT; ++i) {
            glm::vec2 Xi = Hammersley(i, SAMPLE_COUNT);
            glm::vec3 H = ImportanceSampleGGX(Xi, N, roughness);
            glm::vec3 L = glm::normalize(2.0f * glm::dot(V, H) * H - V);

            float NdotL = std::max(L.z, 0.0f);
            float NdotH = std::max(H.z, 0.0f);
            float VdotH = std::max(glm::dot(V, H), 0.0f);

            if (NdotL > 0.0f) {
                float G = GeometrySmith_IBL(N, V, L, roughness);
                float G_Vis = (G * VdotH) / (NdotH * NdotV);
                float Fc = std::pow(1.0f - VdotH, 5.0f);

                A += (1.0f - Fc) * G_Vis;
                B += Fc * G_Vis;
            }
        }
        A /= static_cast<float>(SAMPLE_COUNT);
        B /= static_cast<float>(SAMPLE_COUNT);
        return glm::vec2(A, B);
    }

    TRef<FTexture2D> FIBLGenerator::GenerateBRDFLUT(uint32_t InSize) {
        LE_CORE_INFO("Generating Cook-Torrance 2D BRDF LUT ({0}x{1})...", InSize, InSize);

        std::vector<unsigned char> data(InSize * InSize * 4, 0);

        for (uint32_t y = 0; y < InSize; ++y) {
            float roughness = std::max(static_cast<float>(y) / static_cast<float>(InSize), 0.001f);
            for (uint32_t x = 0; x < InSize; ++x) {
                float NdotV = std::max(static_cast<float>(x) / static_cast<float>(InSize), 0.001f);

                glm::vec2 integrated = IntegrateBRDF(NdotV, roughness);

                uint8_t r = static_cast<uint8_t>(std::clamp(integrated.x * 255.0f, 0.0f, 255.0f));
                uint8_t g = static_cast<uint8_t>(std::clamp(integrated.y * 255.0f, 0.0f, 255.0f));

                size_t index = (y * InSize + x) * 4;
                data[index + 0] = r;
                data[index + 1] = g;
                data[index + 2] = 0;
                data[index + 3] = 255;
            }
        }

        TRef<FTexture2D> lutTexture = FTexture2D::Create(InSize, InSize);
        lutTexture->SetData(data.data(), static_cast<uint32_t>(data.size()));
        return lutTexture;
    }

    FIBLEnvironment FIBLGenerator::CreateEnvironmentFromSkybox(const FSkyboxComponent& InSkybox) {
        FIBLEnvironment env;
        env.BRDFLUT = GenerateBRDFLUT(256);
        env.EnvironmentCubemap = FTextureCube::Create(256, 256, true);
        env.IrradianceMap = FTextureCube::Create(64, 64, true);
        env.PrefilterMap = FTextureCube::Create(128, 128, true);
        return env;
    }

} // namespace Leon
