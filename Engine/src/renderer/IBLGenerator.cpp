#include "renderer/IBLGenerator.hpp"
#include "core/Log.hpp"
#include "renderer/AssetManager.hpp"
#include "renderer/Renderer.hpp"

#include <algorithm>
#include <cmath>
#include <stb_image.h>
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

    static glm::vec3 GetCubeDirection(int face, float u, float v) {
        switch (face) {
        case 0: return glm::normalize(glm::vec3( 1.0f,   -v,   -u)); // +X
        case 1: return glm::normalize(glm::vec3(-1.0f,   -v,    u)); // -X
        case 2: return glm::normalize(glm::vec3(    u, 1.0f,    v)); // +Y
        case 3: return glm::normalize(glm::vec3(    u,-1.0f,   -v)); // -Y
        case 4: return glm::normalize(glm::vec3(    u,   -v, 1.0f)); // +Z
        case 5: return glm::normalize(glm::vec3(   -u,   -v,-1.0f)); // -Z
        default: return glm::vec3(0.0f, 1.0f, 0.0f);
        }
    }

    static glm::vec3 SampleEquirectangular(const float* InHDRData, int InWidth, int InHeight, glm::vec3 InDir) {
        if (!InHDRData || InWidth <= 0 || InHeight <= 0)
            return glm::vec3(0.0f);

        glm::vec3 n = glm::normalize(InDir);
        float u = 0.5f + std::atan2(n.z, n.x) / (2.0f * PI);
        float v = 0.5f - std::asin(std::clamp(n.y, -1.0f, 1.0f)) / PI;
        u = std::clamp(u, 0.0f, 1.0f);
        v = std::clamp(v, 0.0f, 1.0f);

        int x = std::clamp(static_cast<int>(u * (InWidth - 1)), 0, InWidth - 1);
        int y = std::clamp(static_cast<int>(v * (InHeight - 1)), 0, InHeight - 1);
        size_t index = (static_cast<size_t>(y) * InWidth + x) * 4;

        return glm::vec3(InHDRData[index], InHDRData[index + 1], InHDRData[index + 2]);
    }

    static glm::vec3 SampleAtmosphericSky(const FSkyboxComponent& InSkybox, glm::vec3 InDir) {
        glm::vec3 n = glm::normalize(InDir);
        float height = n.y;
        glm::vec3 sky;
        if (height >= 0.0f) {
            float horizonFactor = std::pow(1.0f - height, 4.0f);
            sky = glm::mix(InSkybox.SkyZenithColor, InSkybox.HorizonColor, horizonFactor);
        } else {
            float groundFactor = std::clamp(-height * 3.0f, 0.0f, 1.0f);
            sky = glm::mix(InSkybox.HorizonColor, InSkybox.GroundColor, groundFactor);
        }
        return sky * InSkybox.EnvironmentIntensity;
    }

    FIBLEnvironment FIBLGenerator::CreateEnvironmentFromSkybox(const FSkyboxComponent& InSkybox) {
        FIBLEnvironment env;
        env.BRDFLUT = GenerateBRDFLUT(256);

        // 1. Check if an HDR map is available to load
        float* hdrData = nullptr;
        int hdrWidth = 0, hdrHeight = 0, hdrChannels = 0;
        std::string hdrPath = InSkybox.HDREnvironmentMapPath;
        if (hdrPath.empty() && InSkybox.HDREnvironmentMap) {
            hdrPath = InSkybox.HDREnvironmentMap->GetPath();
        }

        if (InSkybox.bUseHDREnvironmentMap && !hdrPath.empty()) {
            stbi_set_flip_vertically_on_load(1);
            hdrData = stbi_loadf(hdrPath.c_str(), &hdrWidth, &hdrHeight, &hdrChannels, 4);
            if (hdrData) {
                LE_CORE_INFO("FIBLGenerator: Convolving HDR Environment Map from '{0}' ({1}x{2})", hdrPath, hdrWidth,
                             hdrHeight);
            } else {
                LE_CORE_WARN("FIBLGenerator: Failed to load HDR image for convolution: '{0}'", hdrPath);
            }
        }

        auto SampleSky = [&](glm::vec3 InDir) -> glm::vec3 {
            if (hdrData) {
                return SampleEquirectangular(hdrData, hdrWidth, hdrHeight, InDir) * InSkybox.Exposure;
            }
            return SampleAtmosphericSky(InSkybox, InDir);
        };

        // 2. Generate Environment Cubemap (128x128 per face)
        constexpr uint32_t envSize = 128;
        env.EnvironmentCubemap = FTextureCube::Create(envSize, envSize, true);
        {
            std::vector<float> faceBuffer(envSize * envSize * 4);
            for (int face = 0; face < 6; ++face) {
                for (uint32_t y = 0; y < envSize; ++y) {
                    float v = 2.0f * (static_cast<float>(y) + 0.5f) / static_cast<float>(envSize) - 1.0f;
                    for (uint32_t x = 0; x < envSize; ++x) {
                        float u = 2.0f * (static_cast<float>(x) + 0.5f) / static_cast<float>(envSize) - 1.0f;
                        glm::vec3 dir = GetCubeDirection(face, u, v);
                        glm::vec3 color = SampleSky(dir);

                        size_t idx = (y * envSize + x) * 4;
                        faceBuffer[idx + 0] = color.r;
                        faceBuffer[idx + 1] = color.g;
                        faceBuffer[idx + 2] = color.b;
                        faceBuffer[idx + 3] = 1.0f;
                    }
                }
                env.EnvironmentCubemap->SetFaceData(face, faceBuffer.data(), envSize, envSize, 0, true);
            }
            env.EnvironmentCubemap->GenerateMipmaps();
        }

        // 3. Generate Diffuse Irradiance Map (32x32 per face)
        constexpr uint32_t irradSize = 32;
        env.IrradianceMap = FTextureCube::Create(irradSize, irradSize, true);
        {
            std::vector<float> faceBuffer(irradSize * irradSize * 4);
            for (int face = 0; face < 6; ++face) {
                for (uint32_t y = 0; y < irradSize; ++y) {
                    float v = 2.0f * (static_cast<float>(y) + 0.5f) / static_cast<float>(irradSize) - 1.0f;
                    for (uint32_t x = 0; x < irradSize; ++x) {
                        float u = 2.0f * (static_cast<float>(x) + 0.5f) / static_cast<float>(irradSize) - 1.0f;
                        glm::vec3 N = GetCubeDirection(face, u, v);

                        glm::vec3 up = (std::abs(N.z) < 0.999f) ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
                        glm::vec3 tangent = glm::normalize(glm::cross(up, N));
                        glm::vec3 bitangent = glm::cross(N, tangent);

                        glm::vec3 irradiance(0.0f);
                        float sampleDelta = 0.08f;
                        float numSamples = 0.0f;

                        for (float phi = 0.0f; phi < 2.0f * PI; phi += sampleDelta) {
                            for (float theta = 0.0f; theta < 0.5f * PI; theta += sampleDelta) {
                                glm::vec3 tangentSample = glm::vec3(std::sin(theta) * std::cos(phi),
                                                                    std::sin(theta) * std::sin(phi),
                                                                    std::cos(theta));
                                glm::vec3 sampleVec = tangent * tangentSample.x + bitangent * tangentSample.y + N * tangentSample.z;

                                irradiance += SampleSky(sampleVec) * std::cos(theta) * std::sin(theta);
                                numSamples += 1.0f;
                            }
                        }

                        irradiance = PI * irradiance * (1.0f / numSamples);

                        size_t idx = (y * irradSize + x) * 4;
                        faceBuffer[idx + 0] = irradiance.r;
                        faceBuffer[idx + 1] = irradiance.g;
                        faceBuffer[idx + 2] = irradiance.b;
                        faceBuffer[idx + 3] = 1.0f;
                    }
                }
                env.IrradianceMap->SetFaceData(face, faceBuffer.data(), irradSize, irradSize, 0, true);
            }
        }

        // 4. Generate Specular Prefilter Map (128x128, 5 mip levels: 128, 64, 32, 16, 8)
        constexpr uint32_t prefilterBaseSize = 128;
        constexpr uint32_t maxMipLevels = 5;
        env.PrefilterMap = FTextureCube::Create(prefilterBaseSize, prefilterBaseSize, true);
        {
            for (uint32_t mip = 0; mip < maxMipLevels; ++mip) {
                uint32_t mipSize = prefilterBaseSize >> mip;
                float roughness = static_cast<float>(mip) / static_cast<float>(maxMipLevels - 1);
                std::vector<float> faceBuffer(mipSize * mipSize * 4);

                for (int face = 0; face < 6; ++face) {
                    for (uint32_t y = 0; y < mipSize; ++y) {
                        float v = 2.0f * (static_cast<float>(y) + 0.5f) / static_cast<float>(mipSize) - 1.0f;
                        for (uint32_t x = 0; x < mipSize; ++x) {
                            float u = 2.0f * (static_cast<float>(x) + 0.5f) / static_cast<float>(mipSize) - 1.0f;
                            glm::vec3 R = GetCubeDirection(face, u, v);
                            glm::vec3 N = R;
                            glm::vec3 V = R;

                            const uint32_t SAMPLE_COUNT = 128u;
                            glm::vec3 prefilteredColor(0.0f);
                            float totalWeight = 0.0f;

                            for (uint32_t i = 0u; i < SAMPLE_COUNT; ++i) {
                                glm::vec2 Xi = Hammersley(i, SAMPLE_COUNT);
                                glm::vec3 H = ImportanceSampleGGX(Xi, N, roughness);
                                glm::vec3 L = glm::normalize(2.0f * glm::dot(V, H) * H - V);

                                float NdotL = std::max(glm::dot(N, L), 0.0f);
                                if (NdotL > 0.0f) {
                                    prefilteredColor += SampleSky(L) * NdotL;
                                    totalWeight += NdotL;
                                }
                            }

                            prefilteredColor = totalWeight > 0.0f ? prefilteredColor / totalWeight : SampleSky(R);

                            size_t idx = (y * mipSize + x) * 4;
                            faceBuffer[idx + 0] = prefilteredColor.r;
                            faceBuffer[idx + 1] = prefilteredColor.g;
                            faceBuffer[idx + 2] = prefilteredColor.b;
                            faceBuffer[idx + 3] = 1.0f;
                        }
                    }
                    env.PrefilterMap->SetFaceData(face, faceBuffer.data(), mipSize, mipSize, mip, true);
                }
            }
        }

        if (hdrData) {
            stbi_image_free(hdrData);
        }

        LE_CORE_INFO("FIBLGenerator: Real Cook-Torrance IBL Environment generated successfully (Cubemap, Irradiance, Prefilter 5 mips, BRDF LUT).");
        return env;
    }

} // namespace Leon
