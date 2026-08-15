#pragma once

#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>

namespace Leon {

    inline float DistributionGGX(float NdotH, float roughness) {
        float a = roughness * roughness;
        float a2 = a * a;
        float NdotH2 = NdotH * NdotH;

        float nom = a2;
        float denom = (NdotH2 * (a2 - 1.0f) + 1.0f);
        denom = 3.14159265358979323846f * denom * denom;

        return nom / std::max(denom, 0.0000001f);
    }

    inline float GeometrySchlickGGX_Direct(float NdotV, float roughness) {
        float r = (roughness + 1.0f);
        float k = (r * r) / 8.0f;

        float nom = NdotV;
        float denom = NdotV * (1.0f - k) + k;

        return nom / std::max(denom, 0.0000001f);
    }

    inline float GeometrySmith_Direct(float NdotV, float NdotL, float roughness) {
        float ggx2 = GeometrySchlickGGX_Direct(NdotV, roughness);
        float ggx1 = GeometrySchlickGGX_Direct(NdotL, roughness);

        return ggx1 * ggx2;
    }

    inline glm::vec3 FresnelSchlick(float cosTheta, glm::vec3 F0) {
        return F0 + (glm::vec3(1.0f) - F0) * std::pow(std::clamp(1.0f - cosTheta, 0.0f, 1.0f), 5.0f);
    }

    inline glm::vec3 FresnelSchlickRoughness(float cosTheta, glm::vec3 F0, float roughness) {
        return F0 + (glm::max(glm::vec3(1.0f - roughness), F0) - F0) * std::pow(std::clamp(1.0f - cosTheta, 0.0f, 1.0f), 5.0f);
    }

    inline glm::vec3 EvaluateCookTorrance(glm::vec3 N, glm::vec3 V, glm::vec3 L,
                                          glm::vec3 albedo, float metallic, float roughness,
                                          glm::vec3 radiance) {
        glm::vec3 H = glm::normalize(V + L);
        float NdotV = std::max(glm::dot(N, V), 0.0001f);
        float NdotL = std::max(glm::dot(N, L), 0.0f);
        float NdotH = std::max(glm::dot(N, H), 0.0f);
        float HdotV = std::max(glm::dot(H, V), 0.0f);

        if (NdotL <= 0.0f) return glm::vec3(0.0f);

        glm::vec3 F0 = glm::mix(glm::vec3(0.04f), albedo, metallic);

        float NDF = DistributionGGX(NdotH, roughness);
        float G = GeometrySmith_Direct(NdotV, NdotL, roughness);
        glm::vec3 F = FresnelSchlick(HdotV, F0);

        glm::vec3 numerator = NDF * G * F;
        float denominator = 4.0f * NdotV * NdotL;
        glm::vec3 specular = numerator / std::max(denominator, 0.0001f);

        glm::vec3 kS = F;
        glm::vec3 kD = (glm::vec3(1.0f) - kS) * (1.0f - metallic);

        return (kD * albedo / 3.14159265358979323846f + specular) * radiance * NdotL;
    }

    inline bool ValidateEnergyConservation(glm::vec3 kD, glm::vec3 kS, float metallic) {
        if (metallic >= 1.0f) {
            // Pure metals have kD = 0
            if (kD.r > 1e-4f || kD.g > 1e-4f || kD.b > 1e-4f) return false;
            if (kS.r > 1.0001f || kS.g > 1.0001f || kS.b > 1.0001f) return false;
            return true;
        }
        glm::vec3 total = kD + kS;
        return (total.r <= 1.0001f && total.g <= 1.0001f && total.b <= 1.0001f);
    }

} // namespace Leon
