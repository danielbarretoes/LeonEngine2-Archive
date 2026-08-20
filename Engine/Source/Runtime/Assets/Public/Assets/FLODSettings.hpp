#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <vector>

namespace Leon {

    constexpr uint32_t kForcedLODAuto = 0xFFFFFFFFu;
    constexpr uint32_t kMaxStaticMeshLODCount = 8;

    /**
     * One generated static-mesh LOD.
     * MinScreenHeight is the bounding-sphere diameter as a fraction of vertical FOV:
     * 1.0 means the sphere fills the screen vertically. Use this LOD while projected
     * height is >= MinScreenHeight (LOD0 is the largest).
     */
    struct FLODLevel {
        float TriangleRatio = 1.0f;
        float MinScreenHeight = 0.0f;
    };

    struct FLODSettings {
        bool bGenerateLODs = true;
        bool bUseCoarserShadowLOD = true;
        uint32_t MinTriangleCount = 16;
        float Hysteresis = 0.12f;
        std::vector<FLODLevel> Levels = {{1.00f, 0.50f}, {0.60f, 0.25f}, {0.30f, 0.10f}, {0.15f, 0.04f}, {0.05f, 0.00f}};

        static FLODSettings Default() { return {}; }

        uint32_t Hash() const {
            auto mix = [](uint32_t InH, uint32_t InV) {
                InH ^= InV;
                InH *= 16777619u;
                return InH;
            };
            auto mixFloat = [&](uint32_t InH, float InV) {
                uint32_t bits = 0;
                static_assert(sizeof(float) == sizeof(uint32_t));
                std::memcpy(&bits, &InV, sizeof(bits));
                return mix(InH, bits);
            };
            uint32_t h = 2166136261u;
            h = mix(h, bGenerateLODs ? 1u : 0u);
            h = mix(h, bUseCoarserShadowLOD ? 1u : 0u);
            h = mix(h, MinTriangleCount);
            h = mixFloat(h, Hysteresis);
            h = mix(h, static_cast<uint32_t>(Levels.size()));
            for (const auto& level : Levels) {
                h = mixFloat(h, level.TriangleRatio);
                h = mixFloat(h, level.MinScreenHeight);
            }
            return h;
        }
    };

    /** Bounding-sphere diameter as a fraction of the vertical field of view. */
    inline float ComputeProjectedScreenHeight(const glm::vec3& InWorldCenter, float InWorldRadius,
                                              const glm::vec3& InCameraPos, float InFovYDegrees) {
        const float dist = std::max(glm::length(InWorldCenter - InCameraPos), 1.0e-4f);
        const float fov = std::max(InFovYDegrees, 1.0f);
        const float tanHalf = std::tan(fov * 0.5f * (glm::pi<float>() / 180.0f));
        return InWorldRadius / (dist * std::max(tanHalf, 1.0e-4f));
    }

    /**
     * Pick a LOD from projected screen height with hysteresis against InLastLOD.
     * Going coarser requires falling below MinScreenHeight * (1 - hysteresis).
     * Going finer requires rising above MinScreenHeight * (1 + hysteresis).
     */
    inline uint32_t SelectStaticMeshLOD(float InScreenHeight, uint32_t InLastLOD, uint32_t InLODCount,
                                        const FLODSettings& InSettings = FLODSettings::Default()) {
        if (InLODCount <= 1)
            return 0;
        const uint32_t maxIndex = InLODCount - 1;
        const uint32_t levelCount =
            std::min(InLODCount, static_cast<uint32_t>(std::max<size_t>(InSettings.Levels.size(), 1)));
        uint32_t desired = maxIndex;
        for (uint32_t i = 0; i < levelCount; ++i) {
            if (InScreenHeight >= InSettings.Levels[i].MinScreenHeight) {
                desired = i;
                break;
            }
        }
        desired = std::min(desired, maxIndex);
        const uint32_t last = std::min(InLastLOD, maxIndex);
        if (desired == last)
            return last;
        const float h = std::max(InSettings.Hysteresis, 0.0f);
        if (desired > last) {
            const float leave = InSettings.Levels[std::min(last, levelCount - 1)].MinScreenHeight;
            if (InScreenHeight >= leave * (1.0f - h))
                return last;
        } else {
            const float enter = InSettings.Levels[std::min(desired, levelCount - 1)].MinScreenHeight;
            if (InScreenHeight <= enter * (1.0f + h))
                return last;
        }
        return desired;
    }

} // namespace Leon
