#pragma once

#include "Renderer/FPlanarReflectionTypes.hpp"
#include "Renderer/FShadowTypes.hpp"

#include <cstdint>
#include <string>

namespace Leon {

    class UWorld;

    enum class ELeonTournamentGraphicsQuality : uint8_t { Low = 0, Medium = 1, High = 2 };

    struct FLeonTournamentGraphicsPreset {
        uint32_t ShadowMapResolution = 2048;
        uint32_t CascadeCount = 4;
        float ShadowDistance = 100.0f;
        EShadowFilterMode ShadowFilter = EShadowFilterMode::PCF3x3;
        bool bEnablePlanarReflection = true;
        EPlanarReflectionQuality PlanarQuality = EPlanarReflectionQuality::Epic;
        bool bEnableSSAO = true;
        bool bEnableBloom = true;
        bool bEnableFXAA = true;
    };

    struct FLeonTournamentGraphicsQuality {
        static ELeonTournamentGraphicsQuality Parse(const std::string& InValue);
        static const char* ToToken(ELeonTournamentGraphicsQuality InQuality);
        static const char* ToLabel(ELeonTournamentGraphicsQuality InQuality);
        static FLeonTournamentGraphicsPreset GetPreset(ELeonTournamentGraphicsQuality InQuality);
        static ELeonTournamentGraphicsQuality InferFromWorld(const UWorld& InWorld);

        static void ApplyToWorld(UWorld& InWorld, ELeonTournamentGraphicsQuality InQuality);
        static bool PersistToEngineIni(ELeonTournamentGraphicsQuality InQuality);
        static bool PersistToIniFile(ELeonTournamentGraphicsQuality InQuality, const std::string& InIniPath);

        /** Estimated GPU working set for the preset at the given viewport (includes a live asset baseline when
         * available). */
        static size_t EstimateVRAMBytes(ELeonTournamentGraphicsQuality InQuality, uint32_t InViewportWidth,
                                        uint32_t InViewportHeight);
        static std::string FormatVRAMLabel(ELeonTournamentGraphicsQuality InQuality, uint32_t InViewportWidth,
                                           uint32_t InViewportHeight);
    };

} // namespace Leon
