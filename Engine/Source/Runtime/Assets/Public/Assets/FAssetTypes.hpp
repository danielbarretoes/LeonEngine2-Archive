#pragma once

#include "Core/Base.hpp"
#include <cstdint>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace Leon {

    enum class EAssetType : uint8_t {
        Unknown = 0,
        Texture,
        HDREnvironment,
        StaticMesh,
        Material,
        MaterialInstance,
        Shader,
        Level,
        Lightmap
    };

    inline const char* AssetTypeToString(EAssetType InType) {
        switch (InType) {
        case EAssetType::Texture:
            return "Texture";
        case EAssetType::HDREnvironment:
            return "HDREnvironment";
        case EAssetType::StaticMesh:
            return "StaticMesh";
        case EAssetType::Material:
            return "Material";
        case EAssetType::MaterialInstance:
            return "MaterialInstance";
        case EAssetType::Shader:
            return "Shader";
        case EAssetType::Level:
            return "Level";
        case EAssetType::Lightmap:
            return "Lightmap";
        default:
            return "Unknown";
        }
    }

    inline EAssetType StringToAssetType(const std::string& InStr) {
        if (InStr == "Texture")
            return EAssetType::Texture;
        if (InStr == "HDREnvironment" || InStr == "HDR")
            return EAssetType::HDREnvironment;
        if (InStr == "StaticMesh")
            return EAssetType::StaticMesh;
        if (InStr == "Material")
            return EAssetType::Material;
        if (InStr == "MaterialInstance")
            return EAssetType::MaterialInstance;
        if (InStr == "Shader")
            return EAssetType::Shader;
        if (InStr == "Level")
            return EAssetType::Level;
        if (InStr == "Lightmap")
            return EAssetType::Lightmap;
        return EAssetType::Unknown;
    }

    enum class ETextureSemantic : uint8_t {
        Albedo = 0,
        Normal = 1,
        Roughness = 2,
        Metallic = 3,
        AO = 4,
        Emissive = 5,
        Generic = 6
    };

    inline const char* TextureSemanticToString(ETextureSemantic InSemantic) {
        switch (InSemantic) {
        case ETextureSemantic::Albedo:
            return "Albedo";
        case ETextureSemantic::Normal:
            return "Normal";
        case ETextureSemantic::Roughness:
            return "Roughness";
        case ETextureSemantic::Metallic:
            return "Metallic";
        case ETextureSemantic::AO:
            return "AO";
        case ETextureSemantic::Emissive:
            return "Emissive";
        case ETextureSemantic::Generic:
            return "Generic";
        default:
            return "Generic";
        }
    }

    enum class ETextureColorSpace : uint8_t { Linear = 0, sRGB = 1 };

    /**
     * @brief 64-bit / 128-bit Universally Unique Identifier for stable asset references.
     */
    struct FUUID {
        uint64_t High = 0;
        uint64_t Low = 0;

        FUUID() = default;
        FUUID(uint64_t InHigh, uint64_t InLow) : High(InHigh), Low(InLow) {}

        bool IsValid() const { return High != 0 || Low != 0; }

        static FUUID Generate() {
            static std::random_device RandomDevice;
            static std::mt19937_64 Engine(RandomDevice());
            static std::uniform_int_distribution<uint64_t> UniformDistribution;
            return FUUID(UniformDistribution(Engine), UniformDistribution(Engine));
        }

        static FUUID FromString(const std::string& InStr) {
            if (InStr.length() < 32)
                return FUUID();
            std::string highStr = InStr.substr(0, 16);
            std::string lowStr = InStr.substr(16, 16);
            uint64_t h = std::strtoull(highStr.c_str(), nullptr, 16);
            uint64_t l = std::strtoull(lowStr.c_str(), nullptr, 16);
            return FUUID(h, l);
        }

        static FUUID FromPath(const std::string& InPath) {
            // Deterministic 64-bit FNV-1a hash split into UUID
            uint64_t hash = 14695981039346656037ull;
            for (char c : InPath) {
                char lower = (c >= 'A' && c <= 'Z') ? (c + 32) : c;
                if (lower == '\\')
                    lower = '/';
                hash ^= static_cast<uint64_t>(lower);
                hash *= 1099511628211ull;
            }
            return FUUID(0x4C454F4E00000000ull | (hash >> 32), hash); // "LEON" prefix in high bytes
        }

        std::string ToString() const {
            std::stringstream ss;
            ss << std::hex << std::setfill('0') << std::setw(16) << High << std::setw(16) << Low;
            return ss.str();
        }

        bool operator==(const FUUID& InOther) const { return High == InOther.High && Low == InOther.Low; }
        bool operator!=(const FUUID& InOther) const { return !(*this == InOther); }
        bool operator<(const FUUID& InOther) const {
            return High < InOther.High || (High == InOther.High && Low < InOther.Low);
        }
    };

    /**
     * @brief Metadata describing an asset, its origin, virtual path, and dependencies.
     */
    struct FAssetMetadata {
        FUUID ID;
        EAssetType Type = EAssetType::Unknown;
        std::string VirtualPath;               // e.g. "Meshes/SampleMesh.lmesh"
        std::string SourceFilePath;            // e.g. "Assets/Raw/SampleMesh.FBX"
        std::string SourceFileHash;            // SHA-256 / FNV-1a hex string
        std::vector<std::string> Dependencies; // Virtual paths of assets referenced by this asset
        bool bIsLoaded = false;
    };

} // namespace Leon

namespace std {
    template <> struct hash<Leon::FUUID> {
        size_t operator()(const Leon::FUUID& InUUID) const noexcept {
            return std::hash<uint64_t>{}(InUUID.High) ^ (std::hash<uint64_t>{}(InUUID.Low) << 1);
        }
    };
} // namespace std
