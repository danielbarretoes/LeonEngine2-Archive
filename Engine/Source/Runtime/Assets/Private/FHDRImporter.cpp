#include "Assets/FHDRImporter.hpp"
#include "Core/FLog.hpp"
#include "Renderer/FIBLMath.hpp"

#include <stb_image.h>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <cmath>
#include <algorithm>

namespace fs = std::filesystem;

namespace Leon {

    bool FNativeHDRData::SaveToFile(const std::string& InFilePath) const {
        fs::path p(InFilePath);
        if (p.has_parent_path()) {
            fs::create_directories(p.parent_path());
        }

        std::ofstream file(InFilePath, std::ios::binary);
        if (!file.is_open()) {
            LE_CORE_ERROR("FNativeHDRData: Could not open '{0}' for writing", InFilePath);
            return false;
        }

        file.write(reinterpret_cast<const char*>(&Header), sizeof(FLHDRHeader));
        if (!Pixels.empty()) {
            file.write(reinterpret_cast<const char*>(Pixels.data()), Pixels.size() * sizeof(float));
        }

        LE_CORE_INFO("FNativeHDRData: Successfully saved '{0}' ({1}x{2}, {3} KB)", InFilePath, Header.Width,
                     Header.Height, (Pixels.size() * sizeof(float)) / 1024);
        return true;
    }

    bool FNativeHDRData::LoadFromFile(const std::string& InFilePath) {
        std::ifstream file(InFilePath, std::ios::binary);
        if (!file.is_open()) {
            LE_CORE_ERROR("FNativeHDRData: Could not open '{0}' for reading", InFilePath);
            return false;
        }

        file.read(reinterpret_cast<char*>(&Header), sizeof(FLHDRHeader));
        if (Header.Magic != LHDR_MAGIC) {
            LE_CORE_ERROR("FNativeHDRData: Invalid magic in '{0}' (expected 0x{1:X}, got 0x{2:X})", InFilePath,
                          LHDR_MAGIC, Header.Magic);
            return false;
        }

        if (Header.Version != LHDR_VERSION) {
            LE_CORE_ERROR("FNativeHDRData: Unsupported version in '{0}' (expected {1}, got {2})", InFilePath,
                          LHDR_VERSION, Header.Version);
            return false;
        }

        if (Header.Width == 0 || Header.Height == 0 || Header.Width > kMaxCookedHDRDim ||
            Header.Height > kMaxCookedHDRDim || Header.Channels == 0 || Header.Channels > 4) {
            LE_CORE_ERROR("FNativeHDRData: Invalid dimensions/channels in '{0}'", InFilePath);
            return false;
        }
        if (Header.Format != static_cast<uint32_t>(EHDRPixelFormat::RGBA32F)) {
            LE_CORE_ERROR("FNativeHDRData: Unsupported Format {0} in '{1}' (only RGBA32F)", Header.Format, InFilePath);
            return false;
        }

        const uint64_t expectedFloats =
            static_cast<uint64_t>(Header.Width) * static_cast<uint64_t>(Header.Height) * Header.Channels;
        if (expectedFloats > (static_cast<uint64_t>(kMaxCookedHDRDim) * kMaxCookedHDRDim * 4ull)) {
            LE_CORE_ERROR("FNativeHDRData: Payload too large in '{0}'", InFilePath);
            return false;
        }
        if (Header.TotalDataSize != 0 &&
            Header.TotalDataSize != expectedFloats * sizeof(float)) {
            LE_CORE_ERROR("FNativeHDRData: TotalDataSize mismatch in '{0}'", InFilePath);
            return false;
        }

        Pixels.resize(static_cast<size_t>(expectedFloats));
        file.read(reinterpret_cast<char*>(Pixels.data()),
                  static_cast<std::streamsize>(expectedFloats * sizeof(float)));

        if (file.gcount() != static_cast<std::streamsize>(expectedFloats * sizeof(float))) {
            LE_CORE_ERROR("FNativeHDRData: Corrupted or truncated payload in '{0}'", InFilePath);
            return false;
        }

        return true;
    }

    bool FHDRImporter::Import(const std::string& InSourcePath, const FHDRImportSettings& InSettings,
                              FNativeHDRData& OutData) {
        if (!fs::exists(InSourcePath)) {
            LE_CORE_ERROR("FHDRImporter: Source HDR file not found: '{0}'", InSourcePath);
            return false;
        }

        stbi_set_flip_vertically_on_load(InSettings.bFlipVertically ? 1 : 0);

        int width = 0, height = 0, channels = 0;
        float* rawData = stbi_loadf(InSourcePath.c_str(), &width, &height, &channels, 4);
        if (!rawData) {
            LE_CORE_ERROR("FHDRImporter: Failed to parse HDR with stb_image: '{0}' (Reason: {1})", InSourcePath,
                          stbi_failure_reason());
            return false;
        }

        FUUID uuid = FUUID::FromPath(InSourcePath);

        OutData.Header.Magic = LHDR_MAGIC;
        OutData.Header.Version = LHDR_VERSION;
        OutData.Header.UUID_High = uuid.High;
        OutData.Header.UUID_Low = uuid.Low;
        OutData.Header.Width = static_cast<uint32_t>(width);
        OutData.Header.Height = static_cast<uint32_t>(height);
        OutData.Header.Channels = 4;
        OutData.Header.Format = static_cast<uint32_t>(InSettings.TargetFormat);
        OutData.Header.Projection = static_cast<uint32_t>(InSettings.Projection);
        OutData.Header.ColorSpace = 0; // Linear
        OutData.Header.ExposureBias = InSettings.ExposureBias;
        OutData.Header.MipCount = 1;
        OutData.Header.TotalDataSize = static_cast<uint64_t>(width * height * 4 * sizeof(float));

        size_t totalFloats = static_cast<size_t>(width * height * 4);
        OutData.Pixels.resize(totalFloats);
        std::memcpy(OutData.Pixels.data(), rawData, totalFloats * sizeof(float));
        stbi_image_free(rawData);

        float bias = InSettings.ExposureBias;
        if (std::abs(bias - 1.0f) > 1e-6f) {
            for (size_t i = 0; i < totalFloats; i += 4) {
                OutData.Pixels[i + 0] *= bias;
                OutData.Pixels[i + 1] *= bias;
                OutData.Pixels[i + 2] *= bias;
            }
        }
        // Bias already applied to pixels — store remaining scale = 1 so Load never double-applies.
        OutData.Header.ExposureBias = 1.0f;

        LE_CORE_INFO("FHDRImporter: Imported raw HDR '{0}' -> Native .lhdr ({1}x{2}, RGBA32F)", InSourcePath, width,
                     height);
        return true;
    }

    bool FHDRImporter::ImportToFile(const std::string& InSourcePath, const std::string& InDestinationLHDRPath,
                                    const FHDRImportSettings& InSettings) {
        FNativeHDRData data;
        if (!Import(InSourcePath, InSettings, data)) {
            return false;
        }
        return data.SaveToFile(InDestinationLHDRPath);
    }

    bool FHDRImporter::CreateAtmosphericHDR(uint32_t InWidth, uint32_t InHeight, const glm::vec3& InZenith,
                                            const glm::vec3& InHorizon, const glm::vec3& InGround,
                                            const glm::vec3& InSunColor, const glm::vec3& InSunDir,
                                            float InSunIntensity, FNativeHDRData& OutData) {
        if (InWidth == 0 || InHeight == 0)
            return false;

        OutData.Header.Magic = LHDR_MAGIC;
        OutData.Header.Version = LHDR_VERSION;
        FUUID uuid = FUUID::Generate();
        OutData.Header.UUID_High = uuid.High;
        OutData.Header.UUID_Low = uuid.Low;
        OutData.Header.Width = InWidth;
        OutData.Header.Height = InHeight;
        OutData.Header.Channels = 4;
        OutData.Header.Format = static_cast<uint32_t>(EHDRPixelFormat::RGBA32F);
        OutData.Header.Projection = static_cast<uint32_t>(EHDREnvironmentProjection::Equirectangular);
        OutData.Header.ColorSpace = 0; // Linear
        OutData.Header.ExposureBias = 1.0f;
        OutData.Header.MipCount = 1;
        OutData.Header.TotalDataSize = static_cast<uint64_t>(InWidth * InHeight * 4 * sizeof(float));

        OutData.Pixels.resize(InWidth * InHeight * 4);

        glm::vec3 sunNorm = glm::normalize(InSunDir);

        for (uint32_t y = 0; y < InHeight; ++y) {
            float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(InHeight);
            for (uint32_t x = 0; x < InWidth; ++x) {
                float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(InWidth);
                glm::vec3 dir = EquirectDirectionFromUV(u, v);
                glm::vec3 sky = SampleAtmosphericSky(dir, InZenith, InHorizon, InGround);

                // Sun disc hotspot (High dynamic range specular flare)
                float cosAlpha = glm::dot(dir, sunNorm);
                if (cosAlpha > 0.0f) {
                    float sunDisc = std::pow(cosAlpha, 2500.0f) * InSunIntensity * 4.0f;
                    float sunGlow = std::pow(cosAlpha, 32.0f) * InSunIntensity * 0.15f;
                    sky += InSunColor * (sunDisc + sunGlow);
                }

                size_t idx = (static_cast<size_t>(y) * InWidth + x) * 4;
                OutData.Pixels[idx + 0] = sky.r;
                OutData.Pixels[idx + 1] = sky.g;
                OutData.Pixels[idx + 2] = sky.b;
                OutData.Pixels[idx + 3] = 1.0f;
            }
        }

        return true;
    }

} // namespace Leon
