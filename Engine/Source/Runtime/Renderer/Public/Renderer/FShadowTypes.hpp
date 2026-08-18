#pragma once

#include <glm/glm.hpp>
#include <cstdint>

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
     * @brief Configuration parameters for Cascaded Shadow Maps and spotlight shadows.
     * Spotlight shadows: at most one shadowed spot (index 0 of the runtime spot list).
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
        float NormalBias = 0.040f;    ///< Face-normal offset scaling in world space

        uint32_t CascadeResolution = 2048; ///< Width and height per cascade layer
        uint32_t SpotResolution = 1024;    ///< Spotlight shadow map resolution
        int32_t ShadowedSpotIndex = 0;     ///< Runtime spot list index that receives the single spot shadow map
    };

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
