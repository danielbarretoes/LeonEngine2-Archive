#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>

namespace Leon {

    /** Capture size vs viewport. Epic is 1:1 (maximum). */
    enum class EPlanarReflectionQuality : uint8_t { Low = 0, Medium = 1, High = 2, Epic = 3 };

    inline float PlanarReflectionScaleFor(EPlanarReflectionQuality InQuality) {
        switch (InQuality) {
        case EPlanarReflectionQuality::Low:
            return 0.25f;
        case EPlanarReflectionQuality::Medium:
            return 0.50f;
        case EPlanarReflectionQuality::High:
            return 0.75f;
        case EPlanarReflectionQuality::Epic:
        default:
            return 1.0f;
        }
    }

    inline uint32_t PlanarReflectionMipLevelsFor(EPlanarReflectionQuality InQuality) {
        switch (InQuality) {
        case EPlanarReflectionQuality::Low:
            return 3;
        case EPlanarReflectionQuality::Medium:
            return 4;
        case EPlanarReflectionQuality::High:
        case EPlanarReflectionQuality::Epic:
        default:
            return 5;
        }
    }

    inline float ClampPlanarReflectionResolutionScale(float InScale) {
        return std::clamp(InScale, 0.25f, 1.0f);
    }

    /** INI / console tokens: Low, Medium, High, Epic (aliases: Max, Maximum). Default Epic. */
    inline EPlanarReflectionQuality ParsePlanarReflectionQuality(const std::string& InValue) {
        std::string v = InValue;
        for (char& c : v)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (v == "low" || v == "0")
            return EPlanarReflectionQuality::Low;
        if (v == "medium" || v == "med" || v == "1")
            return EPlanarReflectionQuality::Medium;
        if (v == "high" || v == "2")
            return EPlanarReflectionQuality::High;
        return EPlanarReflectionQuality::Epic;
    }

} // namespace Leon
