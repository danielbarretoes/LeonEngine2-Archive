#include "Engine/FGraphicsQuality.hpp"
#include "Engine/UEngine.hpp"
#include "Engine/UWorld.hpp"
#include "Assets/FAssetPath.hpp"
#include "Core/FProjectPaths.hpp"
#include "RHI/FRenderer.hpp"
#include "Renderer/FRenderStats.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <sstream>
#include <utility>
#include <vector>

namespace Leon {

    namespace {
        constexpr const char* kRendererSection = "[/Script/Engine.RendererSettings]";
        constexpr size_t kFallbackAssetBytes = 80ull * 1024ull * 1024ull;

        std::string Lower(std::string InValue) {
            for (char& c : InValue)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return InValue;
        }

        const char* ShadowFilterToken(EShadowFilterMode InFilter) {
            switch (InFilter) {
            case EShadowFilterMode::Hard:
                return "Hard";
            case EShadowFilterMode::PCF5x5:
                return "PCF5x5";
            case EShadowFilterMode::Poisson:
                return "Poisson";
            case EShadowFilterMode::PCF3x3:
            default:
                return "PCF3x3";
            }
        }

        const char* PlanarQualityToken(EPlanarReflectionQuality InQuality) {
            switch (InQuality) {
            case EPlanarReflectionQuality::Low:
                return "Low";
            case EPlanarReflectionQuality::Medium:
                return "Medium";
            case EPlanarReflectionQuality::High:
                return "High";
            case EPlanarReflectionQuality::Epic:
            default:
                return "Epic";
            }
        }

        bool LineIsLiveKey(const std::string& InLine, const std::string& InKey) {
            size_t i = 0;
            while (i < InLine.size() && std::isspace(static_cast<unsigned char>(InLine[i])))
                ++i;
            if (i < InLine.size() && (InLine[i] == ';' || InLine[i] == '#'))
                return false;
            if (InLine.compare(i, InKey.size(), InKey) != 0)
                return false;
            size_t j = i + InKey.size();
            while (j < InLine.size() && std::isspace(static_cast<unsigned char>(InLine[j])))
                ++j;
            return j < InLine.size() && InLine[j] == '=';
        }

        size_t CategoryBytes(EGPUMemoryCategory InCategory) {
            const auto& stats = FRenderer::GetStats();
            return stats.GPUMemory[static_cast<size_t>(InCategory)].Bytes;
        }

        size_t AssetBaselineBytes() {
            const size_t assets =
                CategoryBytes(EGPUMemoryCategory::Texture2D) + CategoryBytes(EGPUMemoryCategory::TextureCube) +
                CategoryBytes(EGPUMemoryCategory::VertexBuffer) + CategoryBytes(EGPUMemoryCategory::IndexBuffer) +
                CategoryBytes(EGPUMemoryCategory::UniformBuffer);
            return assets >= (16ull * 1024ull * 1024ull) ? assets : kFallbackAssetBytes;
        }

        size_t MipChainBytes(size_t InLevel0Bytes, uint32_t InMips) {
            size_t total = 0;
            size_t current = std::max<size_t>(InLevel0Bytes, 1);
            const uint32_t mips = std::max(1u, InMips);
            for (uint32_t i = 0; i < mips; ++i) {
                total += current;
                current = std::max<size_t>(current / 4, 1);
            }
            return total;
        }

        size_t PlanarBytes(uint32_t InViewportW, uint32_t InViewportH, EPlanarReflectionQuality InQuality) {
            const float scale = PlanarReflectionScaleFor(InQuality);
            const uint32_t w = std::max(8u, static_cast<uint32_t>(static_cast<float>(InViewportW) * scale + 0.5f));
            const uint32_t h = std::max(8u, static_cast<uint32_t>(static_cast<float>(InViewportH) * scale + 0.5f));
            const uint32_t mips = PlanarReflectionMipLevelsFor(InQuality);
            const size_t color = MipChainBytes(static_cast<size_t>(w) * h * 8ull, mips);
            const size_t depth = static_cast<size_t>(w) * h * 4ull;
            return 2ull * (color + depth);
        }

        size_t BloomBytes(uint32_t InViewportW, uint32_t InViewportH) {
            size_t total = 0;
            uint32_t w = std::max(InViewportW / 2, 1u);
            uint32_t h = std::max(InViewportH / 2, 1u);
            for (int i = 0; i < 5; ++i) {
                total += 2ull * static_cast<size_t>(w) * h * 8ull;
                w = std::max(w / 2, 1u);
                h = std::max(h / 2, 1u);
            }
            return total;
        }

        size_t SSAOBytes(uint32_t InViewportW, uint32_t InViewportHeight) {
            const uint32_t halfW = std::max(InViewportW / 2, 1u);
            const uint32_t halfH = std::max(InViewportHeight / 2, 1u);
            return 2ull * static_cast<size_t>(halfW) * halfH * 4ull +
                   static_cast<size_t>(InViewportW) * InViewportHeight * 8ull;
        }

        void SyncEngineProjectDefaults(const FGraphicsPreset& InPreset) {
            if (!UEngine::HasInstance())
                return;
            UEngine::Get().SetProjectRendererConfig(InPreset.ShadowMapResolution, InPreset.bEnablePlanarReflection,
                                                    InPreset.CascadeCount, InPreset.ShadowDistance,
                                                    InPreset.PlanarQuality, InPreset.bEnableSSAO, InPreset.bEnableBloom,
                                                    InPreset.bEnableFXAA, InPreset.ShadowFilter);
        }

        bool PresetMatchesWorld(const UWorld& InWorld, const FGraphicsPreset& InPreset) {
            return InWorld.GetPendingShadowMapResolution() == InPreset.ShadowMapResolution &&
                   InWorld.GetPendingCascadeCount() == InPreset.CascadeCount &&
                   InWorld.GetPendingPlanarReflectionEnabled() == InPreset.bEnablePlanarReflection &&
                   InWorld.GetPendingPlanarReflectionQuality() == InPreset.PlanarQuality &&
                   InWorld.GetPendingSSAOEnabled() == InPreset.bEnableSSAO &&
                   InWorld.GetPendingBloomEnabled() == InPreset.bEnableBloom &&
                   InWorld.GetPendingFXAAEnabled() == InPreset.bEnableFXAA &&
                   InWorld.GetPendingShadowFilter() == InPreset.ShadowFilter;
        }
    } // namespace

    EGraphicsQuality FGraphicsQuality::Parse(const std::string& InValue) {
        const std::string v = Lower(InValue);
        if (v == "low" || v == "min" || v == "minimum")
            return EGraphicsQuality::Low;
        if (v == "medium" || v == "med" || v == "recommended")
            return EGraphicsQuality::Medium;
        return EGraphicsQuality::High;
    }

    const char* FGraphicsQuality::ToToken(EGraphicsQuality InQuality) {
        switch (InQuality) {
        case EGraphicsQuality::Low:
            return "Low";
        case EGraphicsQuality::Medium:
            return "Medium";
        case EGraphicsQuality::High:
        default:
            return "High";
        }
    }

    const char* FGraphicsQuality::ToLabel(EGraphicsQuality InQuality) {
        switch (InQuality) {
        case EGraphicsQuality::Low:
            return "LOW";
        case EGraphicsQuality::Medium:
            return "MEDIUM";
        case EGraphicsQuality::High:
        default:
            return "HIGH";
        }
    }

    FGraphicsPreset FGraphicsQuality::GetPreset(EGraphicsQuality InQuality) {
        FGraphicsPreset preset;
        switch (InQuality) {
        case EGraphicsQuality::Low:
            preset.ShadowMapResolution = 512;
            preset.CascadeCount = 1;
            preset.ShadowDistance = 12.0f;
            preset.ShadowFilter = EShadowFilterMode::Hard;
            preset.bEnablePlanarReflection = false;
            preset.PlanarQuality = EPlanarReflectionQuality::Low;
            preset.bEnableSSAO = false;
            preset.bEnableBloom = false;
            preset.bEnableFXAA = false;
            break;
        case EGraphicsQuality::Medium:
            preset.ShadowMapResolution = 1024;
            preset.CascadeCount = 3;
            preset.ShadowDistance = 60.0f;
            preset.ShadowFilter = EShadowFilterMode::PCF3x3;
            preset.bEnablePlanarReflection = true;
            preset.PlanarQuality = EPlanarReflectionQuality::Low;
            preset.bEnableSSAO = true;
            preset.bEnableBloom = true;
            preset.bEnableFXAA = true;
            break;
        case EGraphicsQuality::High:
        default:
            preset.ShadowMapResolution = 2048;
            preset.CascadeCount = 4;
            preset.ShadowDistance = 100.0f;
            preset.ShadowFilter = EShadowFilterMode::PCF5x5;
            preset.bEnablePlanarReflection = true;
            preset.PlanarQuality = EPlanarReflectionQuality::Epic;
            preset.bEnableSSAO = true;
            preset.bEnableBloom = true;
            preset.bEnableFXAA = true;
            break;
        }
        return preset;
    }

    EGraphicsQuality FGraphicsQuality::InferFromWorld(const UWorld& InWorld) {
        if (PresetMatchesWorld(InWorld, GetPreset(EGraphicsQuality::High)))
            return EGraphicsQuality::High;
        if (PresetMatchesWorld(InWorld, GetPreset(EGraphicsQuality::Medium)))
            return EGraphicsQuality::Medium;
        if (PresetMatchesWorld(InWorld, GetPreset(EGraphicsQuality::Low)))
            return EGraphicsQuality::Low;
        if (InWorld.GetPendingShadowMapResolution() >= 2048 &&
            InWorld.GetPendingPlanarReflectionQuality() == EPlanarReflectionQuality::Epic)
            return EGraphicsQuality::High;
        if (InWorld.GetPendingPlanarReflectionEnabled())
            return EGraphicsQuality::Medium;
        return EGraphicsQuality::Low;
    }

    void FGraphicsQuality::ApplyToWorld(UWorld& InWorld, EGraphicsQuality InQuality) {
        // Flow: graphics quality
        // 1. Expand the menu preset into world pending renderer / SSAO / post / filter state
        // 2. Mirror onto UEngine so the next map travel keeps the same working set
        const FGraphicsPreset preset = GetPreset(InQuality);
        InWorld.SetProjectRendererDefaults(preset.ShadowMapResolution, preset.bEnablePlanarReflection,
                                           preset.CascadeCount, preset.ShadowDistance, preset.PlanarQuality, 0.0f);
        InWorld.SetProjectSSAODefaults(preset.bEnableSSAO, InWorld.GetPendingSSAORadius(),
                                       InWorld.GetPendingSSAOIntensity(), InWorld.GetPendingSSAOBias());
        InWorld.SetProjectPostProcessToggles(preset.bEnableBloom, preset.bEnableFXAA);
        InWorld.SetProjectShadowFilter(preset.ShadowFilter);
        SyncEngineProjectDefaults(preset);
    }

    bool FGraphicsQuality::PersistToEngineIni(EGraphicsQuality InQuality) {
        const std::string path = FAssetPath::Combine(FProjectPaths::ProjectConfigDir(), "DefaultEngine.ini");
        if (path.empty() || path == "DefaultEngine.ini")
            return false;
        return PersistToIniFile(InQuality, path);
    }

    bool FGraphicsQuality::PersistToIniFile(EGraphicsQuality InQuality, const std::string& InIniPath) {
        // Patch live keys in place. FConfigFile::Save() would drop the preset comments.
        std::ifstream in(InIniPath, std::ios::binary);
        if (!in)
            return false;
        const std::string original((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        in.close();
        if (original.empty())
            return false;

        const bool crlf = original.find("\r\n") != std::string::npos;
        std::vector<std::string> lines;
        {
            std::istringstream stream(original);
            std::string line;
            while (std::getline(stream, line)) {
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                lines.push_back(std::move(line));
            }
        }

        const FGraphicsPreset preset = GetPreset(InQuality);
        const std::pair<std::string, std::string> keys[] = {
            {"GraphicsQuality", ToToken(InQuality)},
            {"ShadowMapResolution", std::to_string(preset.ShadowMapResolution)},
            {"CascadeCount", std::to_string(preset.CascadeCount)},
            {"ShadowDistance", std::to_string(static_cast<int>(preset.ShadowDistance))},
            {"ShadowFilter", ShadowFilterToken(preset.ShadowFilter)},
            {"EnablePlanarReflection", preset.bEnablePlanarReflection ? "True" : "False"},
            {"PlanarReflectionQuality", PlanarQualityToken(preset.PlanarQuality)},
            {"EnableSSAO", preset.bEnableSSAO ? "True" : "False"},
            {"EnableBloom", preset.bEnableBloom ? "True" : "False"},
            {"EnableFXAA", preset.bEnableFXAA ? "True" : "False"},
        };

        int enableFxaaIndex = -1;
        int rendererSectionIndex = -1;
        for (size_t i = 0; i < lines.size(); ++i) {
            if (lines[i].find(kRendererSection) != std::string::npos)
                rendererSectionIndex = static_cast<int>(i);
            if (LineIsLiveKey(lines[i], "EnableFXAA"))
                enableFxaaIndex = static_cast<int>(i);
        }

        for (const auto& [key, value] : keys) {
            bool replaced = false;
            for (auto& line : lines) {
                if (!LineIsLiveKey(line, key))
                    continue;
                line = key + "=" + value;
                replaced = true;
                break;
            }
            if (replaced || key != "GraphicsQuality")
                continue;

            const std::string insert = "GraphicsQuality=" + value;
            if (enableFxaaIndex >= 0)
                lines.insert(lines.begin() + enableFxaaIndex + 1, insert);
            else if (rendererSectionIndex >= 0)
                lines.insert(lines.begin() + rendererSectionIndex + 1, insert);
            else
                lines.push_back(insert);
        }

        std::ofstream out(InIniPath, std::ios::binary | std::ios::trunc);
        if (!out)
            return false;
        const char* newline = crlf ? "\r\n" : "\n";
        for (size_t i = 0; i < lines.size(); ++i)
            out << lines[i] << newline;
        return static_cast<bool>(out);
    }

    size_t FGraphicsQuality::EstimateVRAMBytes(EGraphicsQuality InQuality, uint32_t InViewportWidth,
                                               uint32_t InViewportHeight) {
        const uint32_t w = std::max(InViewportWidth, 1u);
        const uint32_t h = std::max(InViewportHeight, 1u);
        const FGraphicsPreset preset = GetPreset(InQuality);

        // CSM is always a 4-layer DEPTH32F array even when CascadeCount is 2 or 3.
        size_t bytes = AssetBaselineBytes();
        bytes += static_cast<size_t>(preset.ShadowMapResolution) * preset.ShadowMapResolution * 4ull * 4ull;
        bytes += 1024ull * 1024ull * 4ull;
        if (preset.bEnablePlanarReflection)
            bytes += PlanarBytes(w, h, preset.PlanarQuality);
        bytes += static_cast<size_t>(w) * h * 12ull; // HDR RGBA16F + depth
        bytes += static_cast<size_t>(w) * h * 4ull;  // tone map
        bytes += static_cast<size_t>(w) * h * 4ull;  // swap
        if (preset.bEnableBloom)
            bytes += BloomBytes(w, h);
        if (preset.bEnableSSAO)
            bytes += SSAOBytes(w, h);
        return bytes;
    }

    std::string FGraphicsQuality::FormatVRAMLabel(EGraphicsQuality InQuality, uint32_t InViewportWidth,
                                                  uint32_t InViewportHeight) {
        const size_t mb =
            (EstimateVRAMBytes(InQuality, InViewportWidth, InViewportHeight) + 512ull * 1024ull) / (1024ull * 1024ull);
        char buffer[64];
        std::snprintf(buffer, sizeof(buffer), "%s  ~%zu MB", ToLabel(InQuality), mb);
        return buffer;
    }

} // namespace Leon
