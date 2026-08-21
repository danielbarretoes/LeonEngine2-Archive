#pragma once

#include "Core/Base.hpp"
#include "Assets/FAssetTypes.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace Leon {

    constexpr uint32_t LHDR_MAGIC = 0x5244484C; // 'LHDR' (0x4C, 0x48, 0x44, 0x52)
    constexpr uint32_t LHDR_VERSION = 1;

    enum class EHDREnvironmentProjection : uint32_t { Equirectangular = 0, Cubemap = 1 };

    enum class EHDRPixelFormat : uint32_t { RGBA32F = 0, RGB32F = 1, RGBA16F = 2, RGB16F = 3 };

#pragma pack(push, 1)
    struct FLHDRHeader {
        uint32_t Magic = LHDR_MAGIC;
        uint32_t Version = LHDR_VERSION;
        uint64_t UUID_High = 0;
        uint64_t UUID_Low = 0;
        uint32_t Width = 0;
        uint32_t Height = 0;
        uint32_t Channels = 4;
        /** Only RGBA32F is cooked by FHDRImporter today. */
        uint32_t Format = static_cast<uint32_t>(EHDRPixelFormat::RGBA32F);
        /** Only Equirectangular is cooked by FHDRImporter today. */
        uint32_t Projection = static_cast<uint32_t>(EHDREnvironmentProjection::Equirectangular);
        uint32_t ColorSpace = 0; // 0 = Linear
        /**
         * Remaining exposure scale to apply at runtime. Importer bakes ExposureBias into
         * pixels and writes 1.0 here so Load never double-applies.
         */
        float ExposureBias = 1.0f;
        uint32_t MipCount = 1; // always 1 for cooked .lhdr
        uint64_t TotalDataSize = 0; // float payload size in bytes (W*H*Channels*4)
    };
#pragma pack(pop)
    static_assert(sizeof(FLHDRHeader) == 64, "FLHDRHeader must stay 64 bytes packed");

    /**
     * @brief Container for deserialized native HDR texture asset data.
     */
    struct FNativeHDRData {
        FLHDRHeader Header;
        std::vector<float> Pixels; // 32-bit float RGBA pixels (Width * Height * Channels)

        bool SaveToFile(const std::string& InFilePath) const;
        bool LoadFromFile(const std::string& InFilePath);
    };

    struct FHDRImportSettings {
        EHDREnvironmentProjection Projection = EHDREnvironmentProjection::Equirectangular;
        EHDRPixelFormat TargetFormat = EHDRPixelFormat::RGBA32F;
        float ExposureBias = 1.0f;
        bool bFlipVertically = true; // Radiance HDR standard coordinate convention
    };

    class FHDRImporter {
    public:
        /**
         * @brief Imports a raw Radiance .hdr image and produces a native .lhdr asset container.
         */
        static bool Import(const std::string& InSourcePath, const FHDRImportSettings& InSettings,
                           FNativeHDRData& OutData);

        /**
         * @brief Imports a raw .hdr image and saves directly to destination .lhdr file.
         */
        static bool ImportToFile(const std::string& InSourcePath, const std::string& InDestinationLHDRPath,
                                 const FHDRImportSettings& InSettings);

        /**
         * @brief Synthesizes an atmospheric physical HDR equirectangular map (used for test fixtures & procedural
         * skies).
         */
        static bool CreateAtmosphericHDR(uint32_t InWidth, uint32_t InHeight, const glm::vec3& InZenith,
                                         const glm::vec3& InHorizon, const glm::vec3& InGround,
                                         const glm::vec3& InSunColor, const glm::vec3& InSunDir, float InSunIntensity,
                                         FNativeHDRData& OutData);
    };

} // namespace Leon
