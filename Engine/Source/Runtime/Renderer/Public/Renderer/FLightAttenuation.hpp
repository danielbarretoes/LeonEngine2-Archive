#pragma once

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>

namespace Leon {

    /**
     * UE4/Filament windowed inverse-square. Shared by PBR_Lit.glsl and FLightBaker.
     *   atten = saturate(1 - (d/R)^4)^2 / (d^2 + 1)
     */
    inline float DistanceAttenuationUE4(float InDistance, float InRadius) {
        if (InRadius <= 0.0f)
            return 0.0f;
        float d = std::max(InDistance, 0.0f);
        float ratio = d / InRadius;
        float window = std::clamp(1.0f - ratio * ratio * ratio * ratio, 0.0f, 1.0f);
        window *= window;
        return window / (d * d + 1.0f);
    }

    inline float LambertNdotL(const glm::vec3& InNormal, const glm::vec3& InLightDir) {
        return std::max(glm::dot(InNormal, InLightDir), 0.0f);
    }

    /**
     * Spot angular attenuation matching PBR_Lit.glsl:
     *   t = saturate((cosTheta - cosOuter) / (cosInner - cosOuter))
     *   factor = t^2 * (3 - 2t)   // Hermite smoothstep
     * Inner/outer are cone half-angles in degrees.
     */
    inline float SpotConeAttenuation(const glm::vec3& InLightTravelDir, const glm::vec3& InToLight, float InInnerDeg,
                                     float InOuterDeg) {
        glm::vec3 toLight = InToLight;
        float len2 = glm::dot(toLight, toLight);
        if (len2 < 1e-12f)
            return 0.0f;
        glm::vec3 L = toLight * (1.0f / std::sqrt(len2));
        glm::vec3 travel = glm::normalize(InLightTravelDir);
        float theta = glm::dot(L, -travel);
        float innerCos = std::cos(glm::radians(InInnerDeg));
        float outerCos = std::cos(glm::radians(InOuterDeg));
        float eps = innerCos - outerCos;
        if (std::abs(eps) < 1e-4f)
            eps = (eps < 0.0f) ? -1e-4f : 1e-4f;
        float t = std::clamp((theta - outerCos) / eps, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

} // namespace Leon
