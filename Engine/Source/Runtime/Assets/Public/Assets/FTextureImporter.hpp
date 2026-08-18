#pragma once

#include "Core/Base.hpp"
#include "Assets/FAssetTypes.hpp"
#include "RHI/FTexture.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace Leon {

    constexpr uint32_t LTEX_MAGIC = 0x5845544C; // 'LTEX' in little-endian
    constexpr uint32_t LTEX_VERSION = 1;

#pragma pack(push, 1)
    struct FLTexHeader {
        uint32_t Magic = LTEX_MAGIC;
        uint32_t Version = LTEX_VERSION;
        uint64_t UUID_High = 0;
        uint64_t UUID_Low = 0;
        uint32_t Width = 0;
        uint32_t Height = 0;
        uint32_t Channels = 4;
        uint32_t MipCount = 1;
        uint32_t Format = 0;        // ETextureFormat (0 = RGBA8)
        uint32_t ColorSpace = 0;    // ETextureColorSpace (0 = Linear, 1 = sRGB)
        uint32_t Semantic = 0;      // ETextureSemantic
        uint32_t WrapMode = 0;      // 0 = Repeat, 1 = Clamp
        uint32_t FilterMode = 0;    // 0 = LinearMipmapLinear
        uint64_t TotalDataSize = 0; // Total byte size of all mip payloads
    };

    struct FLTexMipHeader {
        uint32_t Level = 0;
        uint32_t Width = 0;
        uint32_t Height = 0;
        uint32_t DataSize = 0;
    };
#pragma pack(pop)

    struct FTextureMipData {
        uint32_t Level = 0;
        uint32_t Width = 0;
        uint32_t Height = 0;
        std::vector<uint8_t> Pixels;
    };

    struct FNativeTextureData {
        FLTexHeader Header;
        std::vector<FTextureMipData> Mips;

        bool SaveToFile(const std::string& InFilePath) const;
        bool LoadFromFile(const std::string& InFilePath);
    };

    struct FTextureImportSettings {
        ETextureSemantic Semantic = ETextureSemantic::Generic;
        ETextureColorSpace ColorSpace = ETextureColorSpace::sRGB;
        bool bGenerateMipmaps = true;
        bool bInvertGlossToRoughness = false;
        bool bFlipVertically = false;
        uint32_t WrapMode = 0;   // 0 = Repeat, 1 = Clamp
        uint32_t FilterMode = 0; // 0 = LinearMipmapLinear

        static FTextureImportSettings DetectFromFileName(const std::string& InFileName);
    };

    class FTextureImporter {
    public:
        /** Import raw image (PNG/JPG/TGA/HDR) and build native processed texture container */
        static bool Import(const std::string& InSourcePath, const FTextureImportSettings& InSettings,
                           FNativeTextureData& OutData);

        /** Import raw image and save directly to .ltex on disk */
        static bool ImportToFile(const std::string& InSourcePath, const std::string& InDestinationLTexPath,
                                 const FTextureImportSettings& InSettings);

        /** Generate software box-filtered mipmap pyramid from level 0 RGBA8 pixels.
         *  When InbSRGB is true, average in linear light then re-encode IEC sRGB. */
        static void GenerateMipmaps(uint32_t InWidth, uint32_t InHeight, const std::vector<uint8_t>& InLevel0,
                                    std::vector<FTextureMipData>& OutMips, bool InbSRGB = false);

        /** Convert glossiness values to roughness in-place (Roughness = 255 - Gloss) */
        static void ConvertGlossToRoughness(std::vector<uint8_t>& InOutPixels, uint32_t InChannels);
    };

} // namespace Leon
