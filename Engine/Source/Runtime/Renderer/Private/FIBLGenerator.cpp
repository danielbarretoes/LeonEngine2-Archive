#include "Renderer/FIBLGenerator.hpp"
#include "Core/FLog.hpp"
#include "Assets/FHDRImporter.hpp"
#include "Assets/UAssetManager.hpp"
#include "Renderer/FIBLMath.hpp"
#include "RHI/FRenderer.hpp"
#include "RHI/FTexture.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <stb_image.h>
#include <vector>

namespace Leon {

    static TRef<FTexture2D> CachedBRDFLUT = nullptr;
    static uint32_t CachedBRDFLUTSize = 0;

    TRef<FTexture2D> FIBLGenerator::GenerateBRDFLUT(uint32_t InSize) {
        if (CachedBRDFLUT && CachedBRDFLUTSize == InSize) {
            return CachedBRDFLUT;
        }

        auto startT = std::chrono::high_resolution_clock::now();
        std::vector<float> data(InSize * InSize * 2, 0.0f);
        const std::string lutCachePath = "Engine/Assets/Textures/BRDF_LUT.bin";

        bool bLoadedFromDisk = false;
        if (std::filesystem::exists(lutCachePath)) {
            std::ifstream inFile(lutCachePath, std::ios::binary);
            if (inFile.is_open()) {
                FBRDFLUTDiskHeader header{};
                inFile.read(reinterpret_cast<char*>(&header), sizeof(header));
                if (inFile && std::string(header.Magic, 8) == "LEONBRDF" && header.Version == 2 &&
                    header.Size == InSize && header.SampleCount == kBRDFLUTSampleCount) {
                    inFile.read(reinterpret_cast<char*>(data.data()), data.size() * sizeof(float));
                    if (inFile.gcount() == static_cast<std::streamsize>(data.size() * sizeof(float))) {
                        bLoadedFromDisk = true;
                    }
                }
            }
        }

        if (!bLoadedFromDisk) {
            LE_CORE_INFO("Generating Cook-Torrance 2D BRDF LUT ({0}x{1}, RG16F)...", InSize, InSize);
            for (uint32_t y = 0; y < InSize; ++y) {
                float roughness = std::max((static_cast<float>(y) + 0.5f) / static_cast<float>(InSize), 0.001f);
                for (uint32_t x = 0; x < InSize; ++x) {
                    float NdotV = std::max((static_cast<float>(x) + 0.5f) / static_cast<float>(InSize), 0.001f);

                    glm::vec2 integrated = IntegrateBRDF(NdotV, roughness, kBRDFLUTSampleCount);

                    size_t index = (y * InSize + x) * 2;
                    data[index + 0] = integrated.x;
                    data[index + 1] = integrated.y;
                }
            }

            std::filesystem::create_directories("Engine/Assets/Textures");
            std::ofstream outFile(lutCachePath, std::ios::binary);
            if (outFile.is_open()) {
                FBRDFLUTDiskHeader header{};
                header.Size = InSize;
                header.SampleCount = kBRDFLUTSampleCount;
                outFile.write(reinterpret_cast<const char*>(&header), sizeof(header));
                outFile.write(reinterpret_cast<const char*>(data.data()), data.size() * sizeof(float));
            }
        }

        auto lutTexture = FTexture2D::CreateWithFormat(InSize, InSize, ETextureFormat::RG16F);
        if (lutTexture) {
            lutTexture->SetDataFloat(data.data(), static_cast<uint32_t>(data.size() * sizeof(float)));
        }

        CachedBRDFLUT = lutTexture;
        CachedBRDFLUTSize = InSize;
        auto endT = std::chrono::high_resolution_clock::now();
        float durMs = std::chrono::duration<float, std::milli>(endT - startT).count();
        LE_CORE_INFO("  [PROFILE] Cook-Torrance 2D BRDF LUT ({0}x{0}) {1} in {2:.2f} ms", InSize,
                     bLoadedFromDisk ? "loaded from disk cache" : "baked & cached", durMs);
        return CachedBRDFLUT;
    }

    static glm::vec3 SampleSkyboxAtmosphere(const FSkyboxComponent& InSkybox, glm::vec3 InDir) {
        return SampleAtmosphericSky(InDir, InSkybox.SkyZenithColor, InSkybox.HorizonColor, InSkybox.GroundColor);
    }

    static std::string GetIBLCachePath(const std::string& InHDRPath) {
        std::filesystem::path p(InHDRPath);
        std::string stem = p.stem().string();
        std::filesystem::path parentDir = p.parent_path();
        if (!parentDir.empty()) {
            return (parentDir / "Cache" / "IBL" / (stem + ".libl")).string();
        }
        return (std::filesystem::path("Cache/IBL") / (stem + ".libl")).string();
    }

    static bool TryLoadIBLCache(const std::string& InHDRPath, FIBLEnvironment& OutEnv) {
        std::string cachePath = GetIBLCachePath(InHDRPath);
        if (!std::filesystem::exists(cachePath) || !std::filesystem::exists(InHDRPath)) {
            return false;
        }

        uint64_t currentHDRHash = ComputeFileHash64(InHDRPath);

        std::ifstream file(cachePath, std::ios::binary);
        if (!file.is_open())
            return false;

        FIBLCacheHeader header;
        file.read(reinterpret_cast<char*>(&header), sizeof(FIBLCacheHeader));
        if (std::string(header.Magic, 7) != "LEONIBL")
            return false;
        if (header.Version != kIBLCacheVersion) {
            LE_CORE_INFO("FIBLGenerator: Ignoring IBL cache '{0}' (v{1}, need v{2})", cachePath, header.Version,
                         kIBLCacheVersion);
            return false;
        }
        if (header.HDRSourceHash != currentHDRHash || header.SampleCountIrradiance != 512 ||
            header.SampleCountPrefilter != 256) {
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
            OutEnv.IrradianceMap->SetFaceData(face, irradFaceBuffer.data(), header.IrradSize, header.IrradSize, 0,
                                              true);
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

    static void SaveIBLCache(const std::string& InHDRPath, const std::vector<std::vector<float>>& InEnvFaces,
                             const std::vector<std::vector<float>>& InIrradFaces,
                             const std::vector<std::vector<std::vector<float>>>& InPrefilterMips) {
        std::string cachePath = GetIBLCachePath(InHDRPath);
        std::filesystem::create_directories(std::filesystem::path(cachePath).parent_path());

        std::ofstream file(cachePath, std::ios::binary);
        if (!file.is_open())
            return;

        FIBLCacheHeader header;
        header.Version = kIBLCacheVersion;
        header.HDRSourceHash = ComputeFileHash64(InHDRPath);
        file.write(reinterpret_cast<const char*>(&header), sizeof(FIBLCacheHeader));

        // 1. Write Environment Faces
        for (int face = 0; face < 6; ++face) {
            file.write(reinterpret_cast<const char*>(InEnvFaces[face].data()), InEnvFaces[face].size() * sizeof(float));
        }

        // 2. Write Irradiance Faces
        for (int face = 0; face < 6; ++face) {
            file.write(reinterpret_cast<const char*>(InIrradFaces[face].data()),
                       InIrradFaces[face].size() * sizeof(float));
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

        std::string resolvedHdrPath = hdrPath;
        if (!resolvedHdrPath.empty()) {
            resolvedHdrPath = UAssetManager::ResolveVirtualPath(resolvedHdrPath);
        }

        // 1. Check if cached .libl binary asset exists on disk for fast startup (< 5ms)
        if (InSkybox.bUseHDREnvironmentMap && !resolvedHdrPath.empty()) {
            if (TryLoadIBLCache(resolvedHdrPath, env)) {
                auto totalEndT = std::chrono::high_resolution_clock::now();
                float totalDurMs = std::chrono::duration<float, std::milli>(totalEndT - totalStartT).count();
                LE_CORE_INFO("FIBLGenerator: Loaded pre-baked IBL cache (v{0}) for '{1}' in {2:.2f} ms.",
                             kIBLCacheVersion, resolvedHdrPath, totalDurMs);
                return env;
            }
        }

        // 2. Fallback: Full convolution on first load / cache miss
        auto hdrStartT = std::chrono::high_resolution_clock::now();
        const float* hdrData = nullptr;
        float* hdrDataAlloc = nullptr;
        int hdrWidth = 0, hdrHeight = 0, hdrChannels = 0;
        FNativeHDRData nativeHDR;
        FHDREquirectangularMipChain hdrMipChain;

        if (InSkybox.bUseHDREnvironmentMap && !resolvedHdrPath.empty()) {
            if (resolvedHdrPath.length() >= 5 && resolvedHdrPath.substr(resolvedHdrPath.length() - 5) == ".lhdr") {
                if (nativeHDR.LoadFromFile(resolvedHdrPath)) {
                    hdrWidth = static_cast<int>(nativeHDR.Header.Width);
                    hdrHeight = static_cast<int>(nativeHDR.Header.Height);
                    hdrData = nativeHDR.Pixels.data();
                    hdrMipChain.Build(hdrData, hdrWidth, hdrHeight);
                    auto hdrEndT = std::chrono::high_resolution_clock::now();
                    float hdrDurMs = std::chrono::duration<float, std::milli>(hdrEndT - hdrStartT).count();
                    LE_CORE_INFO(
                        "  [PROFILE] Native .lhdr load & mip pyramid '{0}' ({1}x{2}, {3} levels) in {4:.2f} ms",
                        resolvedHdrPath, hdrWidth, hdrHeight, hdrMipChain.Levels.size(), hdrDurMs);
                } else {
                    LE_CORE_WARN("FIBLGenerator: Failed to load native .lhdr asset: '{0}'", resolvedHdrPath);
                }
            } else {
                stbi_set_flip_vertically_on_load(1);
                hdrDataAlloc = stbi_loadf(resolvedHdrPath.c_str(), &hdrWidth, &hdrHeight, &hdrChannels, 4);
                hdrData = hdrDataAlloc;
                auto hdrEndT = std::chrono::high_resolution_clock::now();
                float hdrDurMs = std::chrono::duration<float, std::milli>(hdrEndT - hdrStartT).count();
                if (hdrData) {
                    hdrMipChain.Build(hdrData, hdrWidth, hdrHeight);
                    LE_CORE_INFO("  [PROFILE] Raw HDR load & mip pyramid '{0}' ({1}x{2}, {3} levels) in {4:.2f} ms",
                                 resolvedHdrPath, hdrWidth, hdrHeight, hdrMipChain.Levels.size(), hdrDurMs);
                } else {
                    LE_CORE_WARN("FIBLGenerator: Failed to load raw HDR image: '{0}'", resolvedHdrPath);
                }
            }
        }

        auto SampleSky = [&](glm::vec3 InDir) -> glm::vec3 {
            if (hdrData) {
                return SampleEquirectangular(hdrData, hdrWidth, hdrHeight, InDir);
            }
            return SampleSkyboxAtmosphere(InSkybox, InDir);
        };

        // 3. Generate Environment Cubemap (128x128 per face)
        auto envStartT = std::chrono::high_resolution_clock::now();
        constexpr uint32_t envSize = 128;
        env.EnvironmentCubemap = FTextureCube::Create(envSize, envSize, true);
        std::vector<std::vector<float>> envFaces(6, std::vector<float>(envSize * envSize * 4));
        {
            float envTexelLod = (hdrWidth > 0 && hdrHeight > 0)
                                    ? std::max(0.5f * std::log2(static_cast<float>(hdrWidth * hdrHeight) /
                                                                (6.0f * static_cast<float>(envSize * envSize))),
                                               0.0f)
                                    : 0.0f;

            for (int face = 0; face < 6; ++face) {
                for (uint32_t y = 0; y < envSize; ++y) {
                    float v = 2.0f * (static_cast<float>(y) + 0.5f) / static_cast<float>(envSize) - 1.0f;
                    for (uint32_t x = 0; x < envSize; ++x) {
                        float u = 2.0f * (static_cast<float>(x) + 0.5f) / static_cast<float>(envSize) - 1.0f;
                        glm::vec3 dir = GetCubeDirection(face, u, v);
                        glm::vec3 color;
                        if (hdrData) {
                            color = hdrMipChain.SampleLod(dir, envTexelLod);
                        } else {
                            color = SampleSkyboxAtmosphere(InSkybox, dir);
                        }

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
        LE_CORE_INFO("  [PROFILE] Environment Cubemap (128x128x6, {0} texels) generated in {1:.2f} ms",
                     envSize * envSize * 6, envDurMs);

        // 4. Generate Diffuse Irradiance Map (32x32 per face) with Cosine-Weighted Hemisphere Sampling & Mip Filtering
        auto irradStartT = std::chrono::high_resolution_clock::now();
        constexpr uint32_t irradSize = 32;
        constexpr uint32_t IRRAD_SAMPLE_COUNT = 512u;
        env.IrradianceMap = FTextureCube::Create(irradSize, irradSize, true);
        std::vector<std::vector<float>> irradFaces(6, std::vector<float>(irradSize * irradSize * 4));
        {
            // Karis cubemap texel solid angle of the environment cube (128³ faces), not equirect 4π/(w h).
            const float saTexel = 4.0f * PI / (6.0f * static_cast<float>(envSize * envSize));

            // Solid angle of a sample in cosine-weighted hemisphere sampling:
            // Omega_s = 2*PI / N
            const float irradSaSample = (2.0f * PI) / static_cast<float>(IRRAD_SAMPLE_COUNT);
            const float irradSampleLod = (hdrWidth > 0 && hdrHeight > 0)
                                             ? std::max(0.5f * std::log2(irradSaSample / saTexel) + 1.0f, 0.0f)
                                             : 0.0f;

            for (int face = 0; face < 6; ++face) {
                for (uint32_t y = 0; y < irradSize; ++y) {
                    float v = 2.0f * (static_cast<float>(y) + 0.5f) / static_cast<float>(irradSize) - 1.0f;
                    for (uint32_t x = 0; x < irradSize; ++x) {
                        float u = 2.0f * (static_cast<float>(x) + 0.5f) / static_cast<float>(irradSize) - 1.0f;
                        glm::vec3 N = GetCubeDirection(face, u, v);

                        glm::vec3 up =
                            (std::abs(N.z) < 0.999f) ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
                        glm::vec3 tangent = glm::normalize(glm::cross(up, N));
                        glm::vec3 bitangent = glm::cross(N, tangent);

                        glm::vec3 irradiance(0.0f);

                        for (uint32_t i = 0u; i < IRRAD_SAMPLE_COUNT; ++i) {
                            glm::vec2 Xi = Hammersley(i, IRRAD_SAMPLE_COUNT);
                            float phi = 2.0f * PI * Xi.x;
                            float cosTheta = std::sqrt(1.0f - Xi.y);
                            float sinTheta = std::sqrt(Xi.y);

                            glm::vec3 tangentSample =
                                glm::vec3(sinTheta * std::cos(phi), sinTheta * std::sin(phi), cosTheta);
                            glm::vec3 sampleVec =
                                tangent * tangentSample.x + bitangent * tangentSample.y + N * tangentSample.z;

                            glm::vec3 sampleVal;
                            if (hdrData) {
                                sampleVal = hdrMipChain.SampleLod(sampleVec, irradSampleLod);
                            } else {
                                sampleVal = SampleSkyboxAtmosphere(InSkybox, sampleVec);
                            }

                            irradiance += sampleVal;
                        }

                        irradiance = PI * irradiance * (1.0f / static_cast<float>(IRRAD_SAMPLE_COUNT));

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
        LE_CORE_INFO(
            "  [PROFILE] Irradiance Convolution (32x32x6, 512 samples/px, ~3.1M samples) generated in {0:.2f} ms",
            irradDurMs);

        // 5. Generate Specular Prefilter Map (128x128, 5 mip levels: 128, 64, 32, 16, 8) with Karis PDF Solid Angle
        // Filtering
        constexpr uint32_t prefilterBaseSize = 128;
        constexpr uint32_t maxMipLevels = 5;
        env.PrefilterMap = FTextureCube::Create(prefilterBaseSize, prefilterBaseSize, true);
        std::vector<std::vector<std::vector<float>>> prefilterMips(maxMipLevels);
        {
            // Karis cubemap texel solid angle of the environment cube.
            float saTexel = 4.0f * PI / (6.0f * static_cast<float>(prefilterBaseSize * prefilterBaseSize));

            for (uint32_t mip = 0; mip < maxMipLevels; ++mip) {
                auto mipStartT = std::chrono::high_resolution_clock::now();
                uint32_t mipSize = prefilterBaseSize >> mip;
                float roughness = static_cast<float>(mip) / static_cast<float>(maxMipLevels - 1);
                prefilterMips[mip].resize(6, std::vector<float>(mipSize * mipSize * 4));

                float a = roughness * roughness;
                float a2 = a * a;

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
                                    // Karis PDF LOD with +1.0f mip bias to guarantee solid angle footprint coverage
                                    float sampleLod = (roughness == 0.0f)
                                                          ? 0.0f
                                                          : std::max(0.5f * std::log2(saSample / saTexel) + 1.0f, 0.0f);

                                    glm::vec3 sampleVal;
                                    if (hdrData) {
                                        sampleVal = hdrMipChain.SampleLod(L, sampleLod);
                                    } else {
                                        sampleVal = SampleSkyboxAtmosphere(InSkybox, L);
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
                LE_CORE_INFO(
                    "  [PROFILE] Prefilter Mip {0} ({1}x{1}x6, 256 samples/px, Karis PDF lod) generated in {2:.2f} ms",
                    mip, mipSize, mipDurMs);
            }
        }

        // 6. Save baked IBL result to disk cache for instantaneous future startups
        if (InSkybox.bUseHDREnvironmentMap && !resolvedHdrPath.empty()) {
            SaveIBLCache(resolvedHdrPath, envFaces, irradFaces, prefilterMips);
            LE_CORE_INFO("FIBLGenerator: Saved IBL disk cache (v{0}) to '{1}'", kIBLCacheVersion,
                         GetIBLCachePath(resolvedHdrPath));
        }

        if (hdrDataAlloc) {
            stbi_image_free(hdrDataAlloc);
        }

        auto totalEndT = std::chrono::high_resolution_clock::now();
        float totalDurMs = std::chrono::duration<float, std::milli>(totalEndT - totalStartT).count();
        LE_CORE_INFO("FIBLGenerator: Real Cook-Torrance IBL Environment generated in TOTAL {0:.2f} ms ({1:.2f} s).",
                     totalDurMs, totalDurMs / 1000.0f);
        return env;
    }

} // namespace Leon
