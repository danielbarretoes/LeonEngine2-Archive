#pragma once

#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace Leon {

    inline constexpr float PI = 3.14159265358979323846f;
    inline constexpr float TWO_PI = 6.28318530717958647692f;
    inline constexpr float kAtmosphereGroundFactor = 3.0f;
    inline constexpr uint32_t kBRDFLUTSampleCount = 512u;
    inline constexpr uint32_t kIBLCacheVersion = 6;

    inline float RadicalInverse_VdC(uint32_t bits) {
        bits = (bits << 16u) | (bits >> 16u);
        bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
        bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
        bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
        bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
        return static_cast<float>(bits) * 2.3283064365386963e-10f;
    }

    inline glm::vec2 Hammersley(uint32_t i, uint32_t N) {
        return glm::vec2(static_cast<float>(i) / static_cast<float>(N), RadicalInverse_VdC(i));
    }

    inline glm::vec3 CosineSampleHemisphere(glm::vec2 Xi, glm::vec3 N) {
        float phi = TWO_PI * Xi.x;
        float cosTheta = std::sqrt(std::max(0.0f, 1.0f - Xi.y));
        float sinTheta = std::sqrt(std::max(0.0f, Xi.y));

        glm::vec3 tangentSample(sinTheta * std::cos(phi), sinTheta * std::sin(phi), cosTheta);

        glm::vec3 up = (std::abs(N.z) < 0.999f) ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
        glm::vec3 tangent = glm::normalize(glm::cross(up, N));
        glm::vec3 bitangent = glm::cross(N, tangent);

        glm::vec3 sampleVec = tangent * tangentSample.x + bitangent * tangentSample.y + N * tangentSample.z;
        return glm::normalize(sampleVec);
    }

    inline float CosineHemispherePDF(float cosTheta) {
        return std::max(cosTheta, 0.0f) / PI;
    }

    inline glm::vec3 ImportanceSampleGGX(glm::vec2 Xi, glm::vec3 N, float roughness) {
        float a = roughness * roughness;
        float phi = TWO_PI * Xi.x;
        float cosTheta = std::sqrt((1.0f - Xi.y) / (1.0f + (a * a - 1.0f) * Xi.y));
        float sinTheta = std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));

        glm::vec3 H(std::cos(phi) * sinTheta, std::sin(phi) * sinTheta, cosTheta);

        glm::vec3 up = (std::abs(N.z) < 0.999f) ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
        glm::vec3 tangent = glm::normalize(glm::cross(up, N));
        glm::vec3 bitangent = glm::cross(N, tangent);

        glm::vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
        return glm::normalize(sampleVec);
    }

    inline float GeometrySchlickGGX_IBL(float NdotV, float roughness) {
        float k = (roughness * roughness) / 2.0f;
        float nom = NdotV;
        float denom = NdotV * (1.0f - k) + k;
        return nom / std::max(denom, 0.0000001f);
    }

    inline float GeometrySmith_IBL(glm::vec3 N, glm::vec3 V, glm::vec3 L, float roughness) {
        float NdotV = std::max(glm::dot(N, V), 0.0f);
        float NdotL = std::max(glm::dot(N, L), 0.0f);
        float ggx2 = GeometrySchlickGGX_IBL(NdotV, roughness);
        float ggx1 = GeometrySchlickGGX_IBL(NdotL, roughness);
        return ggx1 * ggx2;
    }

    inline glm::vec2 IntegrateBRDF(float NdotV, float roughness, uint32_t sampleCount = kBRDFLUTSampleCount) {
        glm::vec3 V;
        V.x = std::sqrt(std::max(0.0f, 1.0f - NdotV * NdotV));
        V.y = 0.0f;
        V.z = NdotV;

        float A = 0.0f;
        float B = 0.0f;
        glm::vec3 N = glm::vec3(0.0f, 0.0f, 1.0f);

        for (uint32_t i = 0u; i < sampleCount; ++i) {
            glm::vec2 Xi = Hammersley(i, sampleCount);
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
        A /= static_cast<float>(sampleCount);
        B /= static_cast<float>(sampleCount);
        return glm::vec2(A, B);
    }

    inline glm::vec3 GetCubeDirection(int face, float u, float v) {
        switch (face) {
        case 0:
            return glm::normalize(glm::vec3(1.0f, -v, -u)); // +X
        case 1:
            return glm::normalize(glm::vec3(-1.0f, -v, u)); // -X
        case 2:
            return glm::normalize(glm::vec3(u, 1.0f, v)); // +Y
        case 3:
            return glm::normalize(glm::vec3(u, -1.0f, -v)); // -Y
        case 4:
            return glm::normalize(glm::vec3(u, -v, 1.0f)); // +Z
        case 5:
            return glm::normalize(glm::vec3(-u, -v, -1.0f)); // -Z
        default:
            return glm::vec3(0.0f, 1.0f, 0.0f);
        }
    }

    /** Inverse of SampleEquirectangular UV mapping (OpenGL Y-up). v=0 is +Y, u=0.5 is +X. */
    inline glm::vec3 EquirectDirectionFromUV(float u, float v) {
        float phi = (u - 0.5f) * TWO_PI;
        float y = std::sin((0.5f - v) * PI);
        y = std::clamp(y, -1.0f, 1.0f);
        float horiz = std::sqrt(std::max(0.0f, 1.0f - y * y));
        return glm::vec3(std::cos(phi) * horiz, y, std::sin(phi) * horiz);
    }

    inline glm::vec3 SampleAtmosphericSky(glm::vec3 InDir, const glm::vec3& InZenith, const glm::vec3& InHorizon,
                                          const glm::vec3& InGround) {
        glm::vec3 n = glm::normalize(InDir);
        float height = n.y;
        if (height >= 0.0f) {
            float horizonFactor = std::pow(1.0f - height, 4.0f);
            return glm::mix(InZenith, InHorizon, horizonFactor);
        }
        float groundFactor = std::clamp(-height * kAtmosphereGroundFactor, 0.0f, 1.0f);
        return glm::mix(InHorizon, InGround, groundFactor);
    }

    struct FHDREquirectangularMipChain {
        struct FMipLevel {
            int Width = 0;
            int Height = 0;
            std::vector<float> Data; // RGBA float
        };
        std::vector<FMipLevel> Levels;

        void Build(const float* InBaseData, int InWidth, int InHeight) {
            FMipLevel base;
            base.Width = InWidth;
            base.Height = InHeight;
            base.Data.assign(InBaseData, InBaseData + (static_cast<size_t>(InWidth) * InHeight * 4));
            Levels.push_back(std::move(base));

            int w = InWidth;
            int h = InHeight;
            while (w > 1 || h > 1) {
                int nextW = std::max(1, w / 2);
                int nextH = std::max(1, h / 2);
                const auto& prevData = Levels.back().Data;

                FMipLevel nextMip;
                nextMip.Width = nextW;
                nextMip.Height = nextH;
                nextMip.Data.resize(static_cast<size_t>(nextW) * nextH * 4, 0.0f);

                for (int y = 0; y < nextH; ++y) {
                    for (int x = 0; x < nextW; ++x) {
                        int srcX0 = x * 2;
                        int srcY0 = y * 2;
                        int srcX1 = std::min(srcX0 + 1, w - 1);
                        int srcY1 = std::min(srcY0 + 1, h - 1);

                        auto getPrev = [&](int px, int py) -> glm::vec3 {
                            size_t idx = (static_cast<size_t>(py) * w + px) * 4;
                            return glm::vec3(prevData[idx], prevData[idx + 1], prevData[idx + 2]);
                        };

                        glm::vec3 c00 = getPrev(srcX0, srcY0);
                        glm::vec3 c10 = getPrev(srcX1, srcY0);
                        glm::vec3 c01 = getPrev(srcX0, srcY1);
                        glm::vec3 c11 = getPrev(srcX1, srcY1);
                        glm::vec3 avg = (c00 + c10 + c01 + c11) * 0.25f;

                        size_t dstIdx = (static_cast<size_t>(y) * nextW + x) * 4;
                        nextMip.Data[dstIdx + 0] = avg.r;
                        nextMip.Data[dstIdx + 1] = avg.g;
                        nextMip.Data[dstIdx + 2] = avg.b;
                        nextMip.Data[dstIdx + 3] = 1.0f;
                    }
                }

                Levels.push_back(std::move(nextMip));
                w = nextW;
                h = nextH;
            }
        }

        glm::vec3 SampleLevel(int inLevel, glm::vec3 inDir) const {
            if (Levels.empty())
                return glm::vec3(0.0f);
            int lvl = std::clamp(inLevel, 0, static_cast<int>(Levels.size()) - 1);
            const auto& mip = Levels[lvl];

            glm::vec3 n = glm::normalize(inDir);
            float u = 0.5f + std::atan2(n.z, n.x) / TWO_PI;
            float v = 0.5f - std::asin(std::clamp(n.y, -1.0f, 1.0f)) / PI;
            u = std::fmod(std::fmod(u, 1.0f) + 1.0f, 1.0f);
            v = std::clamp(v, 0.0f, 1.0f);

            float fx = u * static_cast<float>(mip.Width);
            float fy = v * static_cast<float>(mip.Height - 1);

            int x0 = static_cast<int>(std::floor(fx)) % mip.Width;
            if (x0 < 0)
                x0 += mip.Width;
            int x1 = (x0 + 1) % mip.Width;

            int y0 = std::clamp(static_cast<int>(fy), 0, mip.Height - 1);
            int y1 = std::clamp(y0 + 1, 0, mip.Height - 1);

            float tx = fx - std::floor(fx);
            float ty = fy - static_cast<float>(y0);

            auto getTexel = [&](int x, int y) -> glm::vec3 {
                size_t idx = (static_cast<size_t>(y) * mip.Width + x) * 4;
                return glm::vec3(mip.Data[idx], mip.Data[idx + 1], mip.Data[idx + 2]);
            };

            glm::vec3 c00 = getTexel(x0, y0);
            glm::vec3 c10 = getTexel(x1, y0);
            glm::vec3 c01 = getTexel(x0, y1);
            glm::vec3 c11 = getTexel(x1, y1);

            glm::vec3 top = glm::mix(c00, c10, tx);
            glm::vec3 bottom = glm::mix(c01, c11, tx);

            return glm::mix(top, bottom, ty);
        }

        glm::vec3 SampleLod(glm::vec3 inDir, float inLod) const {
            if (Levels.empty())
                return glm::vec3(0.0f);
            if (inLod <= 0.0f)
                return SampleLevel(0, inDir);

            int lvl0 = static_cast<int>(std::floor(inLod));
            int lvl1 = lvl0 + 1;
            float frac = inLod - static_cast<float>(lvl0);

            glm::vec3 s0 = SampleLevel(lvl0, inDir);
            glm::vec3 s1 = SampleLevel(lvl1, inDir);
            return glm::mix(s0, s1, frac);
        }
    };

    inline glm::vec3 SampleEquirectangular(const float* InHDRData, int InWidth, int InHeight, glm::vec3 InDir) {
        if (!InHDRData || InWidth <= 0 || InHeight <= 0)
            return glm::vec3(0.0f);

        glm::vec3 n = glm::normalize(InDir);
        float u = 0.5f + std::atan2(n.z, n.x) / TWO_PI;
        float v = 0.5f - std::asin(std::clamp(n.y, -1.0f, 1.0f)) / PI;
        u = std::fmod(std::fmod(u, 1.0f) + 1.0f, 1.0f);
        v = std::clamp(v, 0.0f, 1.0f);

        float fx = u * static_cast<float>(InWidth);
        float fy = v * static_cast<float>(InHeight - 1);

        int x0 = static_cast<int>(std::floor(fx)) % InWidth;
        if (x0 < 0)
            x0 += InWidth;
        int x1 = (x0 + 1) % InWidth;

        int y0 = std::clamp(static_cast<int>(fy), 0, InHeight - 1);
        int y1 = std::clamp(y0 + 1, 0, InHeight - 1);

        float tx = fx - std::floor(fx);
        float ty = fy - static_cast<float>(y0);

        auto getTexel = [&](int x, int y) -> glm::vec3 {
            size_t idx = (static_cast<size_t>(y) * InWidth + x) * 4;
            return glm::vec3(InHDRData[idx], InHDRData[idx + 1], InHDRData[idx + 2]);
        };

        glm::vec3 c00 = getTexel(x0, y0);
        glm::vec3 c10 = getTexel(x1, y0);
        glm::vec3 c01 = getTexel(x0, y1);
        glm::vec3 c11 = getTexel(x1, y1);

        glm::vec3 top = glm::mix(c00, c10, tx);
        glm::vec3 bottom = glm::mix(c01, c11, tx);

        return glm::mix(top, bottom, ty);
    }

    struct FIBLCacheHeader {
        char Magic[8] = {'L', 'E', 'O', 'N', 'I', 'B', 'L', '\0'};
        uint32_t Version = kIBLCacheVersion; // v6: cubemap saTexel + IEC-aligned env (no exposure baked)
        uint64_t HDRSourceHash = 0;
        uint32_t EnvSize = 128;
        uint32_t IrradSize = 32;
        uint32_t PrefilterBaseSize = 128;
        uint32_t PrefilterMips = 5;
        uint32_t SampleCountIrradiance = 512;
        uint32_t SampleCountPrefilter = 256;
        uint32_t Reserved[4] = {0, 0, 0, 0};
    };
    // Natural alignment (MSVC) pads after Version for uint64 — keep layout stable; do not pack(1).
    static_assert(sizeof(FIBLCacheHeader) == 64, "FIBLCacheHeader must stay 64 bytes on this ABI");

    struct FBRDFLUTDiskHeader {
        char Magic[8] = {'L', 'E', 'O', 'N', 'B', 'R', 'D', 'F'};
        uint32_t Version = 2; // v2: sample count stored; 512-sample IntegrateBRDF
        uint32_t Size = 0;
        uint32_t SampleCount = kBRDFLUTSampleCount;
        uint32_t Reserved = 0;
    };
    static_assert(sizeof(FBRDFLUTDiskHeader) == 24, "BRDF LUT disk header must stay 24 bytes");

    inline uint64_t ComputeFileHash64(const std::string& InFilePath) {
        if (!std::filesystem::exists(InFilePath))
            return 0;
        std::ifstream file(InFilePath, std::ios::binary);
        if (!file.is_open())
            return 0;

        uint64_t hash = 14695981039346656037ull; // FNV-1a 64-bit offset basis
        char buffer[65536];
        while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
            std::streamsize bytesRead = file.gcount();
            for (std::streamsize i = 0; i < bytesRead; ++i) {
                hash ^= static_cast<uint8_t>(buffer[i]);
                hash *= 1099511628211ull; // FNV prime
            }
        }
        return hash;
    }

} // namespace Leon
