#pragma once

#include "Lightmass/FLightmass.hpp"

#include <string>

namespace Leon {

    /**
     * Canonical lighting build presets for playable maps.
     * Preview / Medium (Draft) / High (Production).
     */
    struct ULightBuildSettings {
        ELightingBuildQuality Quality = ELightingBuildQuality::Draft;
        uint32_t Resolution = 64;
        uint32_t IndirectBounces = 2;
        uint32_t Samples = 8;
        bool bDenoise = false;
        float Padding = 2.0f;
        float TexelDensity = 1.0f;

        static ULightBuildSettings Preview() {
            ULightBuildSettings s;
            s.Quality = ELightingBuildQuality::Preview;
            s.Resolution = 32;
            s.IndirectBounces = 1;
            s.Samples = 4;
            return s;
        }
        static ULightBuildSettings Medium() {
            ULightBuildSettings s;
            s.Quality = ELightingBuildQuality::Draft;
            s.Resolution = 64;
            s.IndirectBounces = 2;
            s.Samples = 8;
            return s;
        }
        static ULightBuildSettings High() {
            ULightBuildSettings s;
            s.Quality = ELightingBuildQuality::Production;
            s.Resolution = 128;
            s.IndirectBounces = 3;
            s.Samples = 32;
            return s;
        }

        FLightmassSettings ToLightmass() const {
            FLightmassSettings out;
            ApplyLightingBuildQuality(Quality, out);
            out.LightmapResolution = Resolution;
            out.NumIndirectBounces = IndirectBounces;
            out.SamplesPerTexel = Samples;
            out.TexelPadding = Padding;
            out.WorldScale = TexelDensity;
            return out;
        }
    };

} // namespace Leon
