#include "renderer/IBLGenerator.hpp"
#include "core/Log.hpp"
#include "renderer/AssetManager.hpp"
#include "renderer/Renderer.hpp"
#include "renderer/Texture.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
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

    static TRef<FTexture2D> s_CachedBRDFLUT = nullptr;

    TRef<FTexture2D> FIBLGenerator::GenerateBRDFLUT(uint32_t InSize) {
        if (s_CachedBRDFLUT) {
            return s_CachedBRDFLUT;
        }

        auto startT = std::chrono::high_resolution_clock::now();
        std::vector<float> data(InSize * InSize * 2, 0.0f);
        const std::string lutCachePath = "Engine/Assets/Textures/BRDF_LUT.bin";

        bool bLoadedFromDisk = false;
        if (std::filesystem::exists(lutCachePath)) {
            std::ifstream inFile(lutCachePath, std::ios::binary);
            if (inFile.is_open()) {
                inFile.read(reinterpret_cast<char*>(data.data()), data.size() * sizeof(float));
                if (inFile.gcount() == static_cast<std::streamsize>(data.size() * sizeof(float))) {
                    bLoadedFromDisk = true;
                }
            }
        }

        if (!bLoadedFromDisk) {
            LE_CORE_INFO("Generating Cook-Torrance 2D BRDF LUT ({0}x{1}, RG16F)...", InSize, InSize);
            for (uint32_t y = 0; y < InSize; ++y) {
                float roughness = std::max(static_cast<float>(y) / static_cast<float>(InSize), 0.001f);
                for (uint32_t x = 0; x < InSize; ++x) {
                    float NdotV = std::max(static_cast<float>(x) / static_cast<float>(InSize), 0.001f);

                    glm::vec2 integrated = IntegrateBRDF(NdotV, roughness);

                    size_t index = (y * InSize + x) * 2;
                    data[index + 0] = integrated.x; // Scale (A term)
                    data[index + 1] = integrated.y; // Bias  (B term)
                }
            }

            // Save pre-baked BRDF LUT to disk cache
            std::filesystem::create_directories("Engine/Assets/Textures");
            std::ofstream outFile(lutCachePath, std::ios::binary);
            if (outFile.is_open()) {
                outFile.write(reinterpret_cast<const char*>(data.data()), data.size() * sizeof(float));
            }
        }

        auto lutTexture = FTexture2D::CreateWithFormat(InSize, InSize, ETextureFormat::RG16F);
        if (lutTexture) {
            lutTexture->SetDataFloat(data.data(), static_cast<uint32_t>(data.size() * sizeof(float)));
        }

        s_CachedBRDFLUT = lutTexture;
        auto endT = std::chrono::high_resolution_clock::now();
        float durMs = std::chrono::duration<float, std::milli>(endT - startT).count();
        LE_CORE_INFO("  [PROFILE] Cook-Torrance 2D BRDF LUT ({0}x{0}) {1} in {2:.2f} ms",
                     InSize, bLoadedFromDisk ? "loaded from disk cache" : "baked & cached", durMs);
        return s_CachedBRDFLUT;
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
            while (w > 16 && h > 8) {
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
            if (Levels.empty()) return glm::vec3(0.0f);
            int lvl = std::clamp(inLevel, 0, static_cast<int>(Levels.size()) - 1);
            const auto& mip = Levels[lvl];

            glm::vec3 n = glm::normalize(inDir);
            float u = 0.5f + std::atan2(n.z, n.x) / (2.0f * PI);
            float v = 0.5f - std::asin(std::clamp(n.y, -1.0f, 1.0f)) / PI;
            u = std::clamp(u, 0.0f, 1.0f);
            v = std::clamp(v, 0.0f, 1.0f);

            float fx = u * static_cast<float>(mip.Width - 1);
            float fy = v * static_cast<float>(mip.Height - 1);

            int x0 = static_cast<int>(fx);
            int y0 = static_cast<int>(fy);
            int x1 = std::min(x0 + 1, mip.Width - 1);
            int y1 = std::min(y0 + 1, mip.Height - 1);

            float tx = fx - static_cast<float>(x0);
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
            if (Levels.empty()) return glm::vec3(0.0f);
            if (inLod <= 0.0f) return SampleLevel(0, inDir);

            int lvl0 = static_cast<int>(std::floor(inLod));
            int lvl1 = lvl0 + 1;
            float frac = inLod - static_cast<float>(lvl0);

            glm::vec3 s0 = SampleLevel(lvl0, inDir);
            glm::vec3 s1 = SampleLevel(lvl1, inDir);
            return glm::mix(s0, s1, frac);
        }
    };

    static glm::vec3 SampleEquirectangular(const float* InHDRData, int InWidth, int InHeight, glm::vec3 InDir) {
        if (!InHDRData || InWidth <= 0 || InHeight <= 0)
            return glm::vec3(0.0f);

        glm::vec3 n = glm::normalize(InDir);
        float u = 0.5f + std::atan2(n.z, n.x) / (2.0f * PI);
        float v = 0.5f - std::asin(std::clamp(n.y, -1.0f, 1.0f)) / PI;
        u = std::clamp(u, 0.0f, 1.0f);
        v = std::clamp(v, 0.0f, 1.0f);

        float fx = u * static_cast<float>(InWidth - 1);
        float fy = v * static_cast<float>(InHeight - 1);

        int x0 = static_cast<int>(fx);
        int y0 = static_cast<int>(fy);
        int x1 = std::min(x0 + 1, InWidth - 1);
        int y1 = std::min(y0 + 1, InHeight - 1);

        float tx = fx - static_cast<float>(x0);
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

    struct FIBLCacheHeader {
        char Magic[8] = {'L', 'E', 'O', 'N', 'I', 'B', 'L', '\0'};
        uint32_t Version = 2; // Version 2: Brian Karis PDF solid angle sampling
        uint64_t HDRSourceHash = 0;
        uint32_t EnvSize = 128;
        uint32_t IrradSize = 32;
        uint32_t PrefilterBaseSize = 128;
        uint32_t PrefilterMips = 5;
        uint32_t SampleCountIrradiance = 1580;
        uint32_t SampleCountPrefilter = 256;
        uint32_t Reserved[4] = {0, 0, 0, 0};
    };

    static uint64_t ComputeFileHash64(const std::string& InFilePath) {
        if (!std::filesystem::exists(InFilePath)) return 0;
        std::ifstream file(InFilePath, std::ios::binary);
        if (!file.is_open()) return 0;

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

    static std::string GetIBLCachePath(const std::string& InHDRPath) {
        std::filesystem::path p(InHDRPath);
        std::string stem = p.stem().string();
        return "Projects/Sandbox/Content/Assets/Hdr/Cache/" + stem + ".libl";
    }

    static bool TryLoadIBLCache(const std::string& InHDRPath, FIBLEnvironment& OutEnv) {
        std::string cachePath = GetIBLCachePath(InHDRPath);
        if (!std::filesystem::exists(cachePath) || !std::filesystem::exists(InHDRPath)) {
            return false;
        }

        uint64_t currentHDRHash = ComputeFileHash64(InHDRPath);

        std::ifstream file(cachePath, std::ios::binary);
        if (!file.is_open()) return false;

        FIBLCacheHeader header;
        file.read(reinterpret_cast<char*>(&header), sizeof(FIBLCacheHeader));
        if (std::string(header.Magic, 7) != "LEONIBL" || header.Version != 2 || header.HDRSourceHash != currentHDRHash) {
            return false;
        }

        // 1. Load Environment Cubemap
        OutEnv.EnvironmentCubemap = FTextureCube::Create(header.EnvSize, header.EnvSize, true);
        std::vector<float> envFaceBuffer(header.EnvSize * header.EnvSize * 4);
        for (int face = 0; face < 6; ++face) {
            file.read(reinterpret_cast<char*>(envFaceBuffer.data()), envFaceBuffer.size() * sizeof(float));
            OutEnv.EnvironmentCubemap->SetFaceData(face, envFaceBuffer.data(), header.EnvSize, header.EnvSize, 0, true);
        }
        OutEnv.EnvironmentCubemap->GenerateMipmaps();

        // 2. Load Irradiance Map
        OutEnv.IrradianceMap = FTextureCube::Create(header.IrradSize, header.IrradSize, true);
        std::vector<float> irradFaceBuffer(header.IrradSize * header.IrradSize * 4);
        for (int face = 0; face < 6; ++face) {
            file.read(reinterpret_cast<char*>(irradFaceBuffer.data()), irradFaceBuffer.size() * sizeof(float));
            OutEnv.IrradianceMap->SetFaceData(face, irradFaceBuffer.data(), header.IrradSize, header.IrradSize, 0, true);
        }

        // 3. Load Prefilter Map Mips
        OutEnv.PrefilterMap = FTextureCube::Create(header.PrefilterBaseSize, header.PrefilterBaseSize, true);
        for (uint32_t mip = 0; mip < header.PrefilterMips; ++mip) {
            uint32_t mipSize = header.PrefilterBaseSize >> mip;
            std::vector<float> prefFaceBuffer(mipSize * mipSize * 4);
            for (int face = 0; face < 6; ++face) {
                file.read(reinterpret_cast<char*>(prefFaceBuffer.data()), prefFaceBuffer.size() * sizeof(float));
                OutEnv.PrefilterMap->SetFaceData(face, prefFaceBuffer.data(), mipSize, mipSize, mip, true);
            }
        }

        return true;
    }

    static void SaveIBLCache(const std::string& InHDRPath,
                             const std::vector<std::vector<float>>& InEnvFaces,
                             const std::vector<std::vector<float>>& InIrradFaces,
                             const std::vector<std::vector<std::vector<float>>>& InPrefilterMips) {
        std::string cachePath = GetIBLCachePath(InHDRPath);
        std::filesystem::create_directories(std::filesystem::path(cachePath).parent_path());

        std::ofstream file(cachePath, std::ios::binary);
        if (!file.is_open()) return;

        FIBLCacheHeader header;
        header.Version = 2;
        header.HDRSourceHash = ComputeFileHash64(InHDRPath);
        file.write(reinterpret_cast<const char*>(&header), sizeof(FIBLCacheHeader));

        // 1. Write Environment Faces
        for (int face = 0; face < 6; ++face) {
            file.write(reinterpret_cast<const char*>(InEnvFaces[face].data()), InEnvFaces[face].size() * sizeof(float));
        }

        // 2. Write Irradiance Faces
        for (int face = 0; face < 6; ++face) {
            file.write(reinterpret_cast<const char*>(InIrradFaces[face].data()), InIrradFaces[face].size() * sizeof(float));
        }

        // 3. Write Prefilter Mips
        for (uint32_t mip = 0; mip < header.PrefilterMips; ++mip) {
            for (int face = 0; face < 6; ++face) {
                file.write(reinterpret_cast<const char*>(InPrefilterMips[mip][face].data()),
                           InPrefilterMips[mip][face].size() * sizeof(float));
            }
        }
    }

    FIBLEnvironment FIBLGenerator::CreateEnvironmentFromSkybox(const FSkyboxComponent& InSkybox) {
        auto totalStartT = std::chrono::high_resolution_clock::now();
        FIBLEnvironment env;
        env.BRDFLUT = GenerateBRDFLUT(256);

        std::string hdrPath = InSkybox.HDREnvironmentMapPath;
        if (hdrPath.empty() && InSkybox.HDREnvironmentMap) {
            hdrPath = InSkybox.HDREnvironmentMap->GetPath();
        }

        // 1. Check if cached .libl binary asset exists on disk for fast startup (< 5ms)
        if (InSkybox.bUseHDREnvironmentMap && !hdrPath.empty()) {
            if (TryLoadIBLCache(hdrPath, env)) {
                auto totalEndT = std::chrono::high_resolution_clock::now();
                float totalDurMs = std::chrono::duration<float, std::milli>(totalEndT - totalStartT).count();
                LE_CORE_INFO("FIBLGenerator: Loaded pre-baked IBL cache (v2) for '{0}' in {1:.2f} ms.", hdrPath, totalDurMs);
                return env;
            }
        }

        // 2. Fallback: Full convolution on first load / cache miss
        auto hdrStartT = std::chrono::high_resolution_clock::now();
        float* hdrData = nullptr;
        int hdrWidth = 0, hdrHeight = 0, hdrChannels = 0;
        FHDREquirectangularMipChain hdrMipChain;

        if (InSkybox.bUseHDREnvironmentMap && !hdrPath.empty()) {
            stbi_set_flip_vertically_on_load(1);
            hdrData = stbi_loadf(hdrPath.c_str(), &hdrWidth, &hdrHeight, &hdrChannels, 4);
            auto hdrEndT = std::chrono::high_resolution_clock::now();
            float hdrDurMs = std::chrono::duration<float, std::milli>(hdrEndT - hdrStartT).count();
            if (hdrData) {
                hdrMipChain.Build(hdrData, hdrWidth, hdrHeight);
                LE_CORE_INFO("  [PROFILE] HDR image load & mip pyramid '{0}' ({1}x{2}, {3} levels) in {4:.2f} ms",
                             hdrPath, hdrWidth, hdrHeight, hdrMipChain.Levels.size(), hdrDurMs);
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

        // 3. Generate Environment Cubemap (128x128 per face)
        auto envStartT = std::chrono::high_resolution_clock::now();
        constexpr uint32_t envSize = 128;
        env.EnvironmentCubemap = FTextureCube::Create(envSize, envSize, true);
        std::vector<std::vector<float>> envFaces(6, std::vector<float>(envSize * envSize * 4));
        {
            for (int face = 0; face < 6; ++face) {
                for (uint32_t y = 0; y < envSize; ++y) {
                    float v = 2.0f * (static_cast<float>(y) + 0.5f) / static_cast<float>(envSize) - 1.0f;
                    for (uint32_t x = 0; x < envSize; ++x) {
                        float u = 2.0f * (static_cast<float>(x) + 0.5f) / static_cast<float>(envSize) - 1.0f;
                        glm::vec3 dir = GetCubeDirection(face, u, v);
                        glm::vec3 color = SampleSky(dir);

                        size_t idx = (y * envSize + x) * 4;
                        envFaces[face][idx + 0] = color.r;
                        envFaces[face][idx + 1] = color.g;
                        envFaces[face][idx + 2] = color.b;
                        envFaces[face][idx + 3] = 1.0f;
                    }
                }
                env.EnvironmentCubemap->SetFaceData(face, envFaces[face].data(), envSize, envSize, 0, true);
            }
            env.EnvironmentCubemap->GenerateMipmaps();
        }
        auto envEndT = std::chrono::high_resolution_clock::now();
        float envDurMs = std::chrono::duration<float, std::milli>(envEndT - envStartT).count();
        LE_CORE_INFO("  [PROFILE] Environment Cubemap (128x128x6, {0} texels) generated in {1:.2f} ms", envSize * envSize * 6, envDurMs);

        // 4. Generate Diffuse Irradiance Map (32x32 per face)
        auto irradStartT = std::chrono::high_resolution_clock::now();
        constexpr uint32_t irradSize = 32;
        env.IrradianceMap = FTextureCube::Create(irradSize, irradSize, true);
        std::vector<std::vector<float>> irradFaces(6, std::vector<float>(irradSize * irradSize * 4));
        {
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
                        irradFaces[face][idx + 0] = irradiance.r;
                        irradFaces[face][idx + 1] = irradiance.g;
                        irradFaces[face][idx + 2] = irradiance.b;
                        irradFaces[face][idx + 3] = 1.0f;
                    }
                }
                env.IrradianceMap->SetFaceData(face, irradFaces[face].data(), irradSize, irradSize, 0, true);
            }
        }
        auto irradEndT = std::chrono::high_resolution_clock::now();
        float irradDurMs = std::chrono::duration<float, std::milli>(irradEndT - irradStartT).count();
        LE_CORE_INFO("  [PROFILE] Irradiance Convolution (32x32x6, ~1580 samples/px, ~9.7M samples) generated in {0:.2f} ms", irradDurMs);

        // 5. Generate Specular Prefilter Map (128x128, 5 mip levels: 128, 64, 32, 16, 8) with Karis PDF Solid Angle Filtering
        constexpr uint32_t prefilterBaseSize = 128;
        constexpr uint32_t maxMipLevels = 5;
        env.PrefilterMap = FTextureCube::Create(prefilterBaseSize, prefilterBaseSize, true);
        std::vector<std::vector<std::vector<float>>> prefilterMips(maxMipLevels);
        {
            for (uint32_t mip = 0; mip < maxMipLevels; ++mip) {
                auto mipStartT = std::chrono::high_resolution_clock::now();
                uint32_t mipSize = prefilterBaseSize >> mip;
                float roughness = static_cast<float>(mip) / static_cast<float>(maxMipLevels - 1);
                prefilterMips[mip].resize(6, std::vector<float>(mipSize * mipSize * 4));

                float a = roughness * roughness;
                float a2 = a * a;

                // Solid angle of 1 texel in 128x128 cubemap face
                float saTexel = 4.0f * PI / (6.0f * static_cast<float>(prefilterBaseSize * prefilterBaseSize));

                for (int face = 0; face < 6; ++face) {
                    for (uint32_t y = 0; y < mipSize; ++y) {
                        float v = 2.0f * (static_cast<float>(y) + 0.5f) / static_cast<float>(mipSize) - 1.0f;
                        for (uint32_t x = 0; x < mipSize; ++x) {
                            float u = 2.0f * (static_cast<float>(x) + 0.5f) / static_cast<float>(mipSize) - 1.0f;
                            glm::vec3 R = GetCubeDirection(face, u, v);
                            glm::vec3 N = R;
                            glm::vec3 V = R;

                            const uint32_t SAMPLE_COUNT = 256u;
                            glm::vec3 prefilteredColor(0.0f);
                            float totalWeight = 0.0f;

                            for (uint32_t i = 0u; i < SAMPLE_COUNT; ++i) {
                                glm::vec2 Xi = Hammersley(i, SAMPLE_COUNT);
                                glm::vec3 H = ImportanceSampleGGX(Xi, N, roughness);
                                glm::vec3 L = glm::normalize(2.0f * glm::dot(V, H) * H - V);

                                float NdotL = std::max(glm::dot(N, L), 0.0f);
                                if (NdotL > 0.0f) {
                                    // Brian Karis (Epic Games) PDF Solid Angle Filtering formulation
                                    float NdotH = std::max(glm::dot(N, H), 0.0f);
                                    float VdotH = std::max(glm::dot(V, H), 0.0f);

                                    float denom = (NdotH * NdotH * (a2 - 1.0f) + 1.0f);
                                    float D = a2 / (PI * denom * denom + 0.0000001f);
                                    float pdf = (D * NdotH) / (4.0f * VdotH + 0.0001f) + 0.0001f;

                                    float saSample = 1.0f / (static_cast<float>(SAMPLE_COUNT) * pdf + 0.0001f);
                                    float sampleLod = (roughness == 0.0f) ? 0.0f : std::max(0.5f * std::log2(saSample / saTexel), 0.0f);

                                    glm::vec3 sampleVal;
                                    if (hdrData) {
                                        sampleVal = hdrMipChain.SampleLod(L, sampleLod) * InSkybox.Exposure;
                                    } else {
                                        sampleVal = SampleAtmosphericSky(InSkybox, L);
                                    }

                                    prefilteredColor += sampleVal * NdotL;
                                    totalWeight += NdotL;
                                }
                            }

                            prefilteredColor = totalWeight > 0.0f ? prefilteredColor / totalWeight : SampleSky(R);

                            size_t idx = (y * mipSize + x) * 4;
                            prefilterMips[mip][face][idx + 0] = prefilteredColor.r;
                            prefilterMips[mip][face][idx + 1] = prefilteredColor.g;
                            prefilterMips[mip][face][idx + 2] = prefilteredColor.b;
                            prefilterMips[mip][face][idx + 3] = 1.0f;
                        }
                    }
                    env.PrefilterMap->SetFaceData(face, prefilterMips[mip][face].data(), mipSize, mipSize, mip, true);
                }
                auto mipEndT = std::chrono::high_resolution_clock::now();
                float mipDurMs = std::chrono::duration<float, std::milli>(mipEndT - mipStartT).count();
                LE_CORE_INFO("  [PROFILE] Prefilter Mip {0} ({1}x{1}x6, 256 samples/px, Karis PDF lod) generated in {2:.2f} ms",
                             mip, mipSize, mipDurMs);
            }
        }

        // 6. Save baked IBL result to disk cache for instantaneous future startups
        if (InSkybox.bUseHDREnvironmentMap && !hdrPath.empty()) {
            SaveIBLCache(hdrPath, envFaces, irradFaces, prefilterMips);
            LE_CORE_INFO("FIBLGenerator: Saved IBL disk cache (v2) to '{0}'", GetIBLCachePath(hdrPath));
        }

        if (hdrData) {
            stbi_image_free(hdrData);
        }

        auto totalEndT = std::chrono::high_resolution_clock::now();
        float totalDurMs = std::chrono::duration<float, std::milli>(totalEndT - totalStartT).count();
        LE_CORE_INFO("FIBLGenerator: Real Cook-Torrance IBL Environment generated in TOTAL {0:.2f} ms ({1:.2f} s).", totalDurMs, totalDurMs / 1000.0f);
        return env;
    }

} // namespace Leon
