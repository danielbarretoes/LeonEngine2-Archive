#pragma once

#include <glm/glm.hpp>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

namespace Leon {

    /**
     * Color contract:
     *   MaterialColor  = linear
     *   TextureColor   = hardware-decoded linear (GL_SRGB8_ALPHA8 for sRGB assets)
     *   Lighting       = linear HDR
     *   Display        = tone map then IEC 61966-2-1 sRGB (ToneMapping.glsl)
     *
     * sRGB textures are decoded exactly once by the GPU. Shaders must not pow(rgb, 2.2).
     */
    inline float SRGBToLinear(float InSrgb) {
        InSrgb = std::clamp(InSrgb, 0.0f, 1.0f);
        if (InSrgb <= 0.04045f)
            return InSrgb / 12.92f;
        return std::pow((InSrgb + 0.055f) / 1.055f, 2.4f);
    }

    inline glm::vec3 SRGBToLinear(const glm::vec3& InSrgb) {
        return glm::vec3(SRGBToLinear(InSrgb.r), SRGBToLinear(InSrgb.g), SRGBToLinear(InSrgb.b));
    }

    inline float LinearToSRGB(float InLinear) {
        InLinear = std::max(InLinear, 0.0f);
        if (InLinear <= 0.0031308f)
            return InLinear * 12.92f;
        return 1.055f * std::pow(InLinear, 1.0f / 2.4f) - 0.055f;
    }

    inline glm::vec3 LinearToSRGB(const glm::vec3& InLinear) {
        return glm::vec3(LinearToSRGB(InLinear.r), LinearToSRGB(InLinear.g), LinearToSRGB(InLinear.b));
    }

    /** Data maps (normal, roughness, metallic, AO) stay linear. Color maps use sRGB. */
    inline bool IsLinearDataTexturePath(const std::string& InPath) {
        std::string lower = InPath;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        const char* tags[] = {"_n.",  "_normal", "_norm", "_rough", "_rgh",    "_metal", "_met",
                              "_orm", "_rma",    "_ao.",  "_occ",   "_height", "_disp",  "_bump"};
        for (const char* tag : tags) {
            if (lower.find(tag) != std::string::npos)
                return true;
        }
        return false;
    }

} // namespace Leon
