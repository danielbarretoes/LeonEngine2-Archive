#include "Assets/FLightmapAsset.hpp"
#include "Core/FLog.hpp"
#include "RHI/FTexture.hpp"
#include "Renderer/FIBLMath.hpp"

#include <cstring>
#include <fstream>

namespace Leon {

    void FLightmapAsset::Allocate(uint32_t InWidth, uint32_t InHeight) {
        Header.Width = InWidth;
        Header.Height = InHeight;
        Header.PixelFormat = static_cast<uint32_t>(ELightmapPixelFormat::RGBA32F);
        Header.ChannelCount = 4;
        Header.bIsHDR = 1;
        Header.PayloadSize = InWidth * InHeight * 4 * sizeof(float);
        Pixels.assign(static_cast<size_t>(InWidth) * InHeight * 4, 0.0f);
        GPUTexture.reset();
    }

    void FLightmapAsset::ComputeContentHash() {
        Header.ContentHash = ComputePixelHash();
    }

    uint64_t FLightmapAsset::ComputePixelHash() const {
        uint64_t hash = 14695981039346656037ull;
        auto feed = [&](const void* data, size_t size) {
            const auto* bytes = static_cast<const uint8_t*>(data);
            for (size_t i = 0; i < size; ++i) {
                hash ^= bytes[i];
                hash *= 1099511628211ull;
            }
        };
        feed(&Header.Width, sizeof(Header.Width));
        feed(&Header.Height, sizeof(Header.Height));
        feed(&Header.PixelFormat, sizeof(Header.PixelFormat));
        if (!Pixels.empty())
            feed(Pixels.data(), Pixels.size() * sizeof(float));
        return hash;
    }

    bool FLightmapAsset::SaveToFile(const std::string& InFilePath) const {
        std::ofstream file(InFilePath, std::ios::binary);
        if (!file.is_open()) {
            LE_CORE_ERROR("FLightmapAsset: Failed to write \"{0}\"", InFilePath);
            return false;
        }

        FLightmapHeader header = Header;
        header.Magic = LLIGHTMAP_MAGIC;
        header.Version = LLIGHTMAP_VERSION;
        header.PayloadSize = static_cast<uint32_t>(Pixels.size() * sizeof(float));

        file.write(reinterpret_cast<const char*>(&header), sizeof(header));
        if (!Pixels.empty()) {
            file.write(reinterpret_cast<const char*>(Pixels.data()),
                       static_cast<std::streamsize>(Pixels.size() * sizeof(float)));
        }
        return file.good();
    }

    bool FLightmapAsset::LoadFromFile(const std::string& InFilePath) {
        std::ifstream file(InFilePath, std::ios::binary);
        if (!file.is_open()) {
            LE_CORE_ERROR("FLightmapAsset: Failed to open \"{0}\"", InFilePath);
            return false;
        }

        FLightmapHeader header{};
        file.read(reinterpret_cast<char*>(&header), sizeof(header));
        if (!file.good() || header.Magic != LLIGHTMAP_MAGIC) {
            LE_CORE_ERROR("FLightmapAsset: Invalid magic in \"{0}\"", InFilePath);
            return false;
        }
        if (header.Version != LLIGHTMAP_VERSION) {
            LE_CORE_ERROR("FLightmapAsset: Unsupported version {0} in \"{1}\"", header.Version, InFilePath);
            return false;
        }
        if (header.Width == 0 || header.Height == 0 || header.ChannelCount != 4) {
            LE_CORE_ERROR("FLightmapAsset: Invalid dimensions in \"{0}\"", InFilePath);
            return false;
        }

        const size_t expectedFloats = static_cast<size_t>(header.Width) * header.Height * 4;
        const size_t expectedBytes = expectedFloats * sizeof(float);
        if (header.PayloadSize != expectedBytes) {
            LE_CORE_ERROR("FLightmapAsset: Payload size mismatch in \"{0}\"", InFilePath);
            return false;
        }

        Pixels.resize(expectedFloats);
        file.read(reinterpret_cast<char*>(Pixels.data()), static_cast<std::streamsize>(expectedBytes));
        if (!file.good()) {
            LE_CORE_ERROR("FLightmapAsset: Truncated payload in \"{0}\"", InFilePath);
            return false;
        }

        Header = header;
        AssetPath = InFilePath;
        GPUTexture.reset();
        return true;
    }

    TRef<FTexture2D> FLightmapAsset::GetOrCreateGPUTexture() {
        if (GPUTexture)
            return GPUTexture;
        if (Pixels.empty() || Header.Width == 0 || Header.Height == 0)
            return nullptr;

        GPUTexture = FTexture2D::CreateWithFormat(Header.Width, Header.Height, ETextureFormat::RGBA16F);
        if (!GPUTexture)
            return nullptr;
        GPUTexture->SetDataFloat(Pixels.data(), static_cast<uint32_t>(Pixels.size() * sizeof(float)));
        return GPUTexture;
    }

} // namespace Leon
