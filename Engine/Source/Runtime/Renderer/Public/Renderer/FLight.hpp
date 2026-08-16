#pragma once

#include <glm/glm.hpp>

namespace Leon {

    /**
     * @brief Directional light (sun / infinite distance). Physically-based: single Color + Intensity.
     * The BRDF handles diffuse/specular split — there are no separate per-component intensities.
     */
    struct FDirectionalLight {
        glm::vec3 Direction{-0.5f, -1.0f, -0.3f};
        glm::vec3 Color{1.0f, 0.98f, 0.92f};
        float Intensity{3.0f}; ///< Radiance multiplier (lux equivalent)
    };

    /**
     * @brief Point light with physical inverse-square radius falloff (UE4/Filament model).
     * Attenuation = saturate(1 - (d/Radius)^4)^2 / (d^2 + 1)
     */
    struct FPointLight {
        glm::vec3 Position{0.0f, 1.5f, 0.0f};
        glm::vec3 Color{1.0f, 0.6f, 0.2f}; ///< Warm orange-gold
        float Intensity{8.0f};             ///< Radiance multiplier (candela equivalent)
        float Radius{10.0f};               ///< Effective influence radius in world units
    };

    /**
     * @brief Spot light with cone cutoff and smooth penumbra edges.
     * Same physical radius falloff as point light, clamped to cone angles.
     */
    struct FSpotLight {
        glm::vec3 Position{0.0f, 4.0f, 0.0f};
        glm::vec3 Direction{0.0f, -1.0f, 0.0f};
        glm::vec3 Color{0.2f, 0.8f, 1.0f}; ///< Crisp cyan
        float Intensity{10.0f};            ///< Radiance multiplier
        float Radius{15.0f};               ///< Effective influence radius in world units
        float CutOff{12.5f};               ///< Inner cone angle (degrees)
        float OuterCutOff{17.5f};          ///< Outer cone angle (degrees)
    };

} // namespace Leon
