#pragma once

#include <glm/glm.hpp>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <vector>

namespace Leon {

    /**
     * @brief Shadow filtering algorithms supported across the renderer pipeline.
     */
    enum class EShadowFilterMode : int32_t {
        Hard = 0,   ///< Single tap hardware depth comparison
        PCF3x3 = 1, ///< 9-tap uniform grid kernel
        PCF5x5 = 2, ///< 25-tap uniform grid kernel
        Poisson = 3 ///< 16-tap Poisson disk distribution with interleaved gradient noise
    };

    /**
     * @brief Partitioning strategies for Cascaded Shadow Map slices.
     */
    enum class ECascadeSplitScheme : int32_t {
        Practical = 0,   ///< Blended logarithmic and uniform split (controlled via Lambda)
        Logarithmic = 1, ///< Pure logarithmic distribution (high near-plane resolution)
        Uniform = 2      ///< Equidistant linear distribution
    };

    /**
     * @brief Configuration for CSM, one spotlight map, and up to four point cubemaps.
     */
    struct FShadowSettings {
        bool bEnableShadows = true;
        bool bStabilizeCascades = true;

        EShadowFilterMode FilterMode = EShadowFilterMode::PCF3x3;
        ECascadeSplitScheme SplitScheme = ECascadeSplitScheme::Practical;

        uint32_t CascadeCount = 4;
        float SplitLambda = 0.85f;       ///< Weight between Logarithmic (1.0) and Uniform (0.0)
        float ShadowDistance = 100.0f;   ///< Maximum view distance for directional shadow coverage
        float CascadeBlendWidth = 0.25f; ///< Fraction of each cascade length used as a transition [0.0, 0.5]
        /// Shader also floors the blend to this many metres (keep in sync with PBR_Common.glsl).
        static constexpr float kCascadeBlendMinMeters = 3.0f;

        float ConstantBias = 0.0010f; ///< Constant depth offset subtracted from light depth
        float SlopeBias = 0.0035f;    ///< Multiplier for tan(θ) slope-scale (clamped grazing)
        float NormalBias = 0.055f;    ///< Face-normal offset scaling in world space

        uint32_t CascadeResolution = 2048; ///< Width and height per cascade layer
        uint32_t SpotResolution = 1024;    ///< Spotlight shadow map resolution
        uint32_t PointShadowResolution = 512;
        uint32_t MaxShadowedPointLights = 4;
        int32_t ShadowedSpotIndex = 0; ///< Runtime spot list index that receives the single spot shadow map

        static constexpr uint32_t kMaxShadowedPointLights = 4;
        /// Bind-pose AABB is inflated so animation that leaves rest bounds still casts.
        static constexpr float kSkinnedShadowBoundsPadding = 1.35f;

        /// Closest N skinned meshes may cast shadows. 0 = no count cap.
        uint32_t MaxSkinnedShadowCasters = 4;
        /// Skip skinned casters beyond this camera distance (meters). 0 = no distance cap.
        float SkinnedShadowMaxDistance = 28.0f;
        /// Only the first N CSM slices draw skinned casters. Far cascades stay world-only.
        uint32_t MaxSkinnedShadowCascades = 1;
    };

    struct FSkinnedShadowCasterRank {
        float DistanceSq = 0.0f;
        uint32_t Id = 0;
    };

    /**
     * Keep the closest casters inside the distance budget. InMaxCount 0 disables the count cap;
     * InMaxDistance <= 0 disables the distance cap.
     */
    inline void SelectClosestSkinnedShadowCasters(std::vector<FSkinnedShadowCasterRank>& InOutRanks,
                                                  uint32_t InMaxCount, float InMaxDistance) {
        if (InMaxDistance > 0.0f) {
            const float maxSq = InMaxDistance * InMaxDistance;
            InOutRanks.erase(
                std::remove_if(InOutRanks.begin(), InOutRanks.end(),
                               [maxSq](const FSkinnedShadowCasterRank& InRank) { return InRank.DistanceSq > maxSq; }),
                InOutRanks.end());
        }
        std::sort(InOutRanks.begin(), InOutRanks.end(),
                  [](const FSkinnedShadowCasterRank& InA, const FSkinnedShadowCasterRank& InB) {
                      if (InA.DistanceSq != InB.DistanceSq)
                          return InA.DistanceSq < InB.DistanceSq;
                      return InA.Id < InB.Id;
                  });
        if (InMaxCount > 0 && InOutRanks.size() > InMaxCount)
            InOutRanks.resize(InMaxCount);
    }

    /** INI / console tokens: Hard, PCF3x3, PCF5x5, Poisson. Default PCF3x3. */
    inline EShadowFilterMode ParseShadowFilterMode(const std::string& InValue) {
        std::string v = InValue;
        for (char& c : v)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (v == "hard" || v == "0")
            return EShadowFilterMode::Hard;
        if (v == "pcf5x5" || v == "pcf5" || v == "2")
            return EShadowFilterMode::PCF5x5;
        if (v == "poisson" || v == "3")
            return EShadowFilterMode::Poisson;
        return EShadowFilterMode::PCF3x3;
    }

    /**
     * @brief Runtime state representing a single shadow cascade partition.
     */
    struct FShadowCascade {
        glm::mat4 LightSpaceMatrix{1.0f};
        float SplitNear = 0.1f;
        float SplitFar = 10.0f;
        float WorldUnitsPerTexel = 0.01f;
        glm::vec4 AtlasUVScaleOffset{1.0f, 1.0f, 0.0f, 0.0f}; ///< xy = scale, zw = offset
    };

} // namespace Leon
