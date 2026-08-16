#include "Assets/FTextureImporter.hpp"
#include "Assets/FAssetPath.hpp"
#include "Core/FLog.hpp"

#include <stb_image.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>

namespace Leon {

    bool FNativeTextureData::SaveToFile(const std::string& InFilePath) const {
        std::ofstream file(InFilePath, std::ios::binary);
        if (!file.is_open()) {
            LE_CORE_ERROR("FNativeTextureData: Failed to open \"{0}\" for writing", InFilePath);
            return false;
        }

        // Calculate total payload size
        FLTexHeader header = Header;
        header.MipCount = static_cast<uint32_t>(Mips.size());
        uint64_t totalSize = 0;
        for (const auto& mip : Mips) {
            totalSize += sizeof(FLTexMipHeader) + mip.Pixels.size();
        }
        header.TotalDataSize = totalSize;

        // Write header
        file.write(reinterpret_cast<const char*>(&header), sizeof(FLTexHeader));

        // Write mips
        for (const auto& mip : Mips) {
            FLTexMipHeader mipHeader;
            mipHeader.Level = mip.Level;
            mipHeader.Width = mip.Width;
            mipHeader.Height = mip.Height;
            mipHeader.DataSize = static_cast<uint32_t>(mip.Pixels.size());

            file.write(reinterpret_cast<const char*>(&mipHeader), sizeof(FLTexMipHeader));
            if (!mip.Pixels.empty()) {
                file.write(reinterpret_cast<const char*>(mip.Pixels.data()), mip.Pixels.size());
            }
        }

        return file.good();
    }

    bool FNativeTextureData::LoadFromFile(const std::string& InFilePath) {
        std::ifstream file(InFilePath, std::ios::binary);
        if (!file.is_open()) {
            LE_CORE_ERROR("FNativeTextureData: Failed to open \"{0}\" for reading", InFilePath);
            return false;
        }

        file.read(reinterpret_cast<char*>(&Header), sizeof(FLTexHeader));
        if (!file.good()) {
            LE_CORE_ERROR("FNativeTextureData: Failed to read header from \"{0}\"", InFilePath);
            return false;
        }

        if (Header.Magic != LTEX_MAGIC) {
            LE_CORE_ERROR("FNativeTextureData: Invalid magic in \"{0}\" (got {1:x}, expected {2:x})", InFilePath,
                          Header.Magic, LTEX_MAGIC);
            return false;
        }

        if (Header.Version != LTEX_VERSION) {
            LE_CORE_ERROR("FNativeTextureData: Unsupported version in \"{0}\" ({1})", InFilePath, Header.Version);
            return false;
        }

        Mips.clear();
        Mips.resize(Header.MipCount);

        for (uint32_t i = 0; i < Header.MipCount; ++i) {
            FLTexMipHeader mipHeader;
            file.read(reinterpret_cast<char*>(&mipHeader), sizeof(FLTexMipHeader));
            if (!file.good()) {
                LE_CORE_ERROR("FNativeTextureData: Corrupt mip {0} header in \"{1}\"", i, InFilePath);
                return false;
            }

            Mips[i].Level = mipHeader.Level;
            Mips[i].Width = mipHeader.Width;
            Mips[i].Height = mipHeader.Height;
            Mips[i].Pixels.resize(mipHeader.DataSize);

            if (mipHeader.DataSize > 0) {
                file.read(reinterpret_cast<char*>(Mips[i].Pixels.data()), mipHeader.DataSize);
                if (!file.good()) {
                    LE_CORE_ERROR("FNativeTextureData: Corrupt mip {0} data in \"{1}\"", i, InFilePath);
                    return false;
                }
            }
        }

        return true;
    }

    FTextureImportSettings FTextureImportSettings::DetectFromFileName(const std::string& InFileName) {
        FTextureImportSettings settings;
        std::string lower = InFileName;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        if (lower.find("_diff") != std::string::npos || lower.find("_color") != std::string::npos ||
            lower.find("_albedo") != std::string::npos || lower.find("_basecolor") != std::string::npos) {
            settings.Semantic = ETextureSemantic::Albedo;
            settings.ColorSpace = ETextureColorSpace::sRGB;
        } else if (lower.find("_normal") != std::string::npos || lower.find("_nmap") != std::string::npos ||
                   lower.find("_nrm") != std::string::npos || lower.find("_norm") != std::string::npos) {
            settings.Semantic = ETextureSemantic::Normal;
            settings.ColorSpace = ETextureColorSpace::Linear;
        } else if (lower.find("_gloss") != std::string::npos) {
            settings.Semantic = ETextureSemantic::Roughness;
            settings.ColorSpace = ETextureColorSpace::Linear;
            settings.bInvertGlossToRoughness = true;
        } else if (lower.find("_rough") != std::string::npos || lower.find("_roughness") != std::string::npos) {
            settings.Semantic = ETextureSemantic::Roughness;
            settings.ColorSpace = ETextureColorSpace::Linear;
        } else if (lower.find("_metal") != std::string::npos || lower.find("_metallic") != std::string::npos ||
                   lower.find("_met") != std::string::npos) {
            settings.Semantic = ETextureSemantic::Metallic;
            settings.ColorSpace = ETextureColorSpace::Linear;
        } else if (lower.find("_ao") != std::string::npos || lower.find("_occlusion") != std::string::npos ||
                   lower.find("_ambient") != std::string::npos) {
            settings.Semantic = ETextureSemantic::AO;
            settings.ColorSpace = ETextureColorSpace::Linear;
        } else if (lower.find("_illum") != std::string::npos || lower.find("_emissive") != std::string::npos ||
                   lower.find("_emit") != std::string::npos) {
            settings.Semantic = ETextureSemantic::Emissive;
            settings.ColorSpace = ETextureColorSpace::sRGB;
        } else {
            settings.Semantic = ETextureSemantic::Generic;
            settings.ColorSpace = ETextureColorSpace::Linear;
        }

        return settings;
    }

    bool FTextureImporter::Import(const std::string& InSourcePath, const FTextureImportSettings& InSettings,
                                  FNativeTextureData& OutData) {
        int width = 0, height = 0, channels = 0;
        stbi_set_flip_vertically_on_load(InSettings.bFlipVertically ? 1 : 0);

        stbi_uc* rawPixels = stbi_load(InSourcePath.c_str(), &width, &height, &channels, 4);
        if (!rawPixels) {
            LE_CORE_ERROR("FTextureImporter: stb_image failed to load \"{0}\"", InSourcePath);
            return false;
        }

        uint32_t uWidth = static_cast<uint32_t>(width);
        uint32_t uHeight = static_cast<uint32_t>(height);
        size_t byteCount = static_cast<size_t>(uWidth * uHeight * 4);

        std::vector<uint8_t> level0(byteCount);
        std::memcpy(level0.data(), rawPixels, byteCount);
        stbi_image_free(rawPixels);

        // Convert glossiness to roughness if requested
        if (InSettings.bInvertGlossToRoughness) {
            ConvertGlossToRoughness(level0, 4);
        }

        // Setup Header
        FUUID uuid = FUUID::FromPath(FAssetPath::GetFileName(InSourcePath));
        OutData.Header.Magic = LTEX_MAGIC;
        OutData.Header.Version = LTEX_VERSION;
        OutData.Header.UUID_High = uuid.High;
        OutData.Header.UUID_Low = uuid.Low;
        OutData.Header.Width = uWidth;
        OutData.Header.Height = uHeight;
        OutData.Header.Channels = 4;
        OutData.Header.Format = 0; // RGBA8
        OutData.Header.ColorSpace = static_cast<uint32_t>(InSettings.ColorSpace);
        OutData.Header.Semantic = static_cast<uint32_t>(InSettings.Semantic);
        OutData.Header.WrapMode = InSettings.WrapMode;
        OutData.Header.FilterMode = InSettings.FilterMode;

        // Generate Mipmap Pyramid
        if (InSettings.bGenerateMipmaps) {
            GenerateMipmaps(uWidth, uHeight, level0, OutData.Mips);
        } else {
            OutData.Mips.clear();
            FTextureMipData mip0;
            mip0.Level = 0;
            mip0.Width = uWidth;
            mip0.Height = uHeight;
            mip0.Pixels = std::move(level0);
            OutData.Mips.push_back(std::move(mip0));
        }

        OutData.Header.MipCount = static_cast<uint32_t>(OutData.Mips.size());
        return true;
    }

    bool FTextureImporter::ImportToFile(const std::string& InSourcePath, const std::string& InDestinationLTexPath,
                                        const FTextureImportSettings& InSettings) {
        FNativeTextureData data;
        if (!Import(InSourcePath, InSettings, data)) {
            return false;
        }
        return data.SaveToFile(InDestinationLTexPath);
    }

    void FTextureImporter::GenerateMipmaps(uint32_t InWidth, uint32_t InHeight, const std::vector<uint8_t>& InLevel0,
                                           std::vector<FTextureMipData>& OutMips) {
        OutMips.clear();

        uint32_t numLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(InWidth, InHeight)))) + 1;
        OutMips.reserve(numLevels);

        // Mip 0
        FTextureMipData mip0;
        mip0.Level = 0;
        mip0.Width = InWidth;
        mip0.Height = InHeight;
        mip0.Pixels = InLevel0;
        OutMips.push_back(std::move(mip0));

        uint32_t curW = InWidth;
        uint32_t curH = InHeight;

        for (uint32_t level = 1; level < numLevels; ++level) {
            uint32_t nextW = std::max(1u, curW / 2);
            uint32_t nextH = std::max(1u, curH / 2);

            FTextureMipData nextMip;
            nextMip.Level = level;
            nextMip.Width = nextW;
            nextMip.Height = nextH;
            nextMip.Pixels.resize(nextW * nextH * 4);

            const auto& prevPixels = OutMips[level - 1].Pixels;

            for (uint32_t y = 0; y < nextH; ++y) {
                for (uint32_t x = 0; x < nextW; ++x) {
                    uint32_t srcX0 = x * 2;
                    uint32_t srcX1 = std::min(srcX0 + 1, curW - 1);
                    uint32_t srcY0 = y * 2;
                    uint32_t srcY1 = std::min(srcY0 + 1, curH - 1);

                    for (uint32_t c = 0; c < 4; ++c) {
                        uint32_t p00 = prevPixels[(srcY0 * curW + srcX0) * 4 + c];
                        uint32_t p10 = prevPixels[(srcY0 * curW + srcX1) * 4 + c];
                        uint32_t p01 = prevPixels[(srcY1 * curW + srcX0) * 4 + c];
                        uint32_t p11 = prevPixels[(srcY1 * curW + srcX1) * 4 + c];

                        uint32_t avg = (p00 + p10 + p01 + p11 + 2) / 4;
                        nextMip.Pixels[(y * nextW + x) * 4 + c] = static_cast<uint8_t>(avg);
                    }
                }
            }

            OutMips.push_back(std::move(nextMip));
            curW = nextW;
            curH = nextH;
        }
    }

    void FTextureImporter::ConvertGlossToRoughness(std::vector<uint8_t>& InOutPixels, uint32_t InChannels) {
        size_t pixelCount = InOutPixels.size() / InChannels;
        for (size_t i = 0; i < pixelCount; ++i) {
            // Invert RGB channels (or R channel if single-channel data)
            InOutPixels[i * InChannels + 0] = static_cast<uint8_t>(255 - InOutPixels[i * InChannels + 0]);
            if (InChannels >= 3) {
                InOutPixels[i * InChannels + 1] = static_cast<uint8_t>(255 - InOutPixels[i * InChannels + 1]);
                InOutPixels[i * InChannels + 2] = static_cast<uint8_t>(255 - InOutPixels[i * InChannels + 2]);
            }
        }
    }

} // namespace Leon
