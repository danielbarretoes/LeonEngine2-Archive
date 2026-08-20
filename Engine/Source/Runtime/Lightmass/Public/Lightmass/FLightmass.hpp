#pragma once

#include "Assets/FLightmapAsset.hpp"
#include "Engine/EMobility.hpp"
#include "Renderer/FLight.hpp"
#include "Engine/Components.hpp"

#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace Leon {

    class UWorld;

    struct FLightmassSettings {
        uint32_t LightmapResolution = 64;
        uint32_t NumIndirectBounces = 2;
        uint32_t SamplesPerTexel = 16;
        float IndirectIntensity = 1.0f;
        bool bAmbientOcclusion = true;
        float AOIntensity = 1.0f;
        float AORadius = 1.0f;
        float WorldScale = 1.0f;
        uint64_t DeterministicSeed = 0x4C454F4E4C4D4153ull; // "LEONLMAS"
        float TexelPadding = 2.0f;
        ELightingBuildQuality LightingBuildQuality = ELightingBuildQuality::Draft;
    };

    inline void ApplyLightingBuildQuality(ELightingBuildQuality InQuality, FLightmassSettings& OutSettings) {
        OutSettings.LightingBuildQuality = InQuality;
        switch (InQuality) {
        case ELightingBuildQuality::Preview:
            OutSettings.LightmapResolution = 32;
            OutSettings.SamplesPerTexel = 4;
            OutSettings.NumIndirectBounces = 1;
            break;
        case ELightingBuildQuality::Production:
            OutSettings.LightmapResolution = 128;
            OutSettings.SamplesPerTexel = 32;
            OutSettings.NumIndirectBounces = 3;
            break;
        case ELightingBuildQuality::Draft:
        default:
            OutSettings.LightmapResolution = 64;
            OutSettings.SamplesPerTexel = 8;
            OutSettings.NumIndirectBounces = 2;
            break;
        }
    }

    struct FLightmassBakeResult {
        bool bSuccess = false;
        std::string Message;
        std::string LightmapPath;
        uint64_t BakeHash = 0;
        uint32_t StaticMeshCount = 0;
        uint32_t StaticLightCount = 0;
        uint32_t AtlasWidth = 0;
        uint32_t AtlasHeight = 0;
    };

    /**
     * @brief Offline static lighting (Unreal Lightmass analogue).
     * Does not run inside FWorldRenderer. Invoked by LightmassTool.
     *
     * Flow: load map → filter Static receivers + Static/Stationary lights → UV1 →
     * atlas → CPU bake → write .llightmap → stamp .lmap metadata (hash excludes that metadata).
     */
    class FLightmass {
    public:
        static FLightmassBakeResult BakeMap(const std::string& InMapPath, const FLightmassSettings& InSettings,
                                            bool bForce = false);

        static bool ValidateMap(const std::string& InMapPath, std::string& OutMessage);

        /**
         * Stable bake-input fingerprint from world + settings (ignores LightmapBakeHash /
         * LightmapScale/Bias/Index/Asset stamped by a previous bake).
         */
        static uint64_t ComputeBakeInputHash(const UWorld& InWorld, const FLightmassSettings& InSettings);

        /** Compare stored LightmapBakeHash to the current world. Disables sampling when stale. */
        static void RefreshRuntimeLightmapTrust(UWorld& InWorld);
    };

} // namespace Leon
