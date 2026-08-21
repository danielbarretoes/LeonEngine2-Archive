#pragma once

#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

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
     * Aim axis in actor local space is -Y (matches default Direction). Actor rotation drives aim.
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

    /** Local axis the spot aims along when actor rotation is identity. */
    inline glm::vec3 SpotLightLocalAimAxis() {
        return glm::vec3(0.0f, -1.0f, 0.0f);
    }

    inline glm::vec3 SpotLightWorldDirectionFromEuler(const glm::vec3& InEulerDegrees) {
        const glm::quat q(glm::radians(InEulerDegrees));
        glm::vec3 d = q * SpotLightLocalAimAxis();
        const float len = glm::length(d);
        return len > 1e-6f ? d / len : SpotLightLocalAimAxis();
    }

    /** Euler degrees (Pitch,Yaw,Roll) that aim local -Y along InWorldDir. */
    inline glm::vec3 SpotLightEulerFromWorldDirection(const glm::vec3& InWorldDir) {
        glm::vec3 to = InWorldDir;
        const float toLen = glm::length(to);
        to = toLen > 1e-6f ? to / toLen : SpotLightLocalAimAxis();

        const glm::vec3 from = SpotLightLocalAimAxis();
        const float d = glm::clamp(glm::dot(from, to), -1.0f, 1.0f);
        glm::quat q(1.0f, 0.0f, 0.0f, 0.0f);
        if (d < -0.99999f) {
            glm::vec3 axis = std::abs(from.x) < 0.9f ? glm::cross(from, glm::vec3(1.0f, 0.0f, 0.0f))
                                                     : glm::cross(from, glm::vec3(0.0f, 0.0f, 1.0f));
            q = glm::angleAxis(glm::pi<float>(), glm::normalize(axis));
        } else if (d < 0.99999f) {
            q = glm::angleAxis(std::acos(d), glm::normalize(glm::cross(from, to)));
        }
        return glm::degrees(glm::eulerAngles(q));
    }

    inline void SyncSpotLightFromTransform(FSpotLight& InOutLight, const glm::vec3& InWorldPos,
                                           const glm::vec3& InEulerDegrees) {
        InOutLight.Position = InWorldPos;
        InOutLight.Direction = SpotLightWorldDirectionFromEuler(InEulerDegrees);
    }

} // namespace Leon
