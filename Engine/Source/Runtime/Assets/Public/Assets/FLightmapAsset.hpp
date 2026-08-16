#pragma once

#include "Core/Base.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace Leon {

    constexpr uint32_t LLIGHTMAP_MAGIC = 0x4D4C4C4C; // 'LLLM' little-endian mnemonic
    constexpr uint32_t LLIGHTMAP_VERSION = 2;

    enum class ELightmapPixelFormat : uint32_t {
        RGBA16F = 1, ///< HDR irradiance (preferred)
        RGBA32F = 2
    };

    /**
     * @brief Native lightmap atlas asset (.llightmap). Stores baked irradiance (not final shaded color).
     */
    struct FLightmapHeader {
        uint32_t Magic = LLIGHTMAP_MAGIC;
        uint32_t Version = LLIGHTMAP_VERSION;
        uint32_t Width = 0;
        uint32_t Height = 0;
        uint32_t PixelFormat = static_cast<uint32_t>(ELightmapPixelFormat::RGBA32F);
        uint32_t ChannelCount = 4;
        uint32_t bIsHDR = 1;
        uint32_t PayloadSize = 0;
        uint64_t ContentHash = 0;
        uint32_t Reserved[4] = {0, 0, 0, 0};
    };

    class FLightmapAsset {
    public:
        FLightmapAsset() = default;

        uint32_t GetWidth() const { return Header.Width; }
        uint32_t GetHeight() const { return Header.Height; }
        ELightmapPixelFormat GetPixelFormat() const {
            return static_cast<ELightmapPixelFormat>(Header.PixelFormat);
        }
        bool IsHDR() const { return Header.bIsHDR != 0; }
        uint64_t GetContentHash() const { return Header.ContentHash; }
        void SetContentHash(uint64_t InHash) { Header.ContentHash = InHash; }
        const FLightmapHeader& GetHeader() const { return Header; }

        const std::vector<float>& GetPixelsRGBA32F() const { return Pixels; }
        std::vector<float>& GetPixelsRGBA32F() { return Pixels; }

        void Allocate(uint32_t InWidth, uint32_t InHeight);
        void ComputeContentHash();

        bool SaveToFile(const std::string& InFilePath) const;
        bool LoadFromFile(const std::string& InFilePath);

        /** Create / refresh GPU texture (RGBA16F) from float payload. Lazy; may return null offline. */
        TRef<class FTexture2D> GetOrCreateGPUTexture();

        void InvalidateGPU() { GPUTexture.reset(); }

    private:
        FLightmapHeader Header;
        std::vector<float> Pixels; ///< RGBA32F linear irradiance
        TRef<class FTexture2D> GPUTexture;
        std::string AssetPath;
    };

} // namespace Leon
