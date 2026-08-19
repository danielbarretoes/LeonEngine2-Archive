#pragma once

#include "Renderer/FPlanarReflectionTypes.hpp"
#include "Renderer/FShadowTypes.hpp"

#include <cstdint>
#include <string>

namespace Leon {

    class UWorld;

    enum class EGraphicsQuality : uint8_t { Low = 0, Medium = 1, High = 2 };

    struct FGraphicsPreset {
        uint32_t ShadowMapResolution = 2048;
        uint32_t CascadeCount = 4;
        uint32_t SpotResolution = 1024;
        uint32_t PointShadowResolution = 512;
        uint32_t MaxShadowedPointLights = 4;
        float ShadowDistance = 100.0f;
        EShadowFilterMode ShadowFilter = EShadowFilterMode::PCF3x3;
        bool bEnablePlanarReflection = true;
        EPlanarReflectionQuality PlanarQuality = EPlanarReflectionQuality::Epic;
        bool bEnableSSAO = true;
        bool bEnableBloom = true;
        bool bEnableFXAA = true;
    };

    /** Low / Medium / High renderer presets (product-agnostic). */
    struct FGraphicsQuality {
        static EGraphicsQuality Parse(const std::string& InValue);
        static const char* ToToken(EGraphicsQuality InQuality);
        static const char* ToLabel(EGraphicsQuality InQuality);
        static FGraphicsPreset GetPreset(EGraphicsQuality InQuality);
        static EGraphicsQuality InferFromWorld(const UWorld& InWorld);

        static void ApplyToWorld(UWorld& InWorld, EGraphicsQuality InQuality);
        static bool PersistToEngineIni(EGraphicsQuality InQuality);
        static bool PersistToIniFile(EGraphicsQuality InQuality, const std::string& InIniPath);

        /** Estimated GPU working set for the preset at the given viewport (includes a live asset baseline when
         * available). */
        static size_t EstimateVRAMBytes(EGraphicsQuality InQuality, uint32_t InViewportWidth, uint32_t InViewportHeight);
        static std::string FormatVRAMLabel(EGraphicsQuality InQuality, uint32_t InViewportWidth,
                                           uint32_t InViewportHeight);
    };

} // namespace Leon
