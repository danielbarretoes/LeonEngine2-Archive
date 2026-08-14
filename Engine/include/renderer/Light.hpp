#pragma once

#include <glm/glm.hpp>

namespace Leon {

    /**
     * @brief Directional light properties (Sunlight / infinite distance light).
     */
    struct FDirectionalLight {
        glm::vec3 Direction{-0.5f, -1.0f, -0.3f};
        glm::vec3 Color{1.0f, 0.98f, 0.92f};
        float AmbientIntensity{0.15f};
        float DiffuseIntensity{0.7f};
        float SpecularIntensity{0.4f};
    };

    /**
     * @brief Point light properties with distance attenuation.
     */
    struct FPointLight {
        glm::vec3 Position{0.0f, 1.5f, 0.0f};
        glm::vec3 Color{1.0f, 0.6f, 0.2f}; // Warm orange-gold
        float Constant{1.0f};
        float Linear{0.09f};
        float Quadratic{0.032f};
        float AmbientIntensity{0.05f};
        float DiffuseIntensity{1.0f};
        float SpecularIntensity{1.0f};
    };

    /**
     * @brief Spot light properties with cone cutoff and smooth penumbra edges.
     */
    struct FSpotLight {
        glm::vec3 Position{0.0f, 4.0f, 0.0f};
        glm::vec3 Direction{0.0f, -1.0f, 0.0f};
        glm::vec3 Color{0.2f, 0.8f, 1.0f}; // Crisp cyan
        float CutOff{12.5f};               // Inner cone angle (degrees)
        float OuterCutOff{17.5f};          // Outer cone angle (degrees)
        float Constant{1.0f};
        float Linear{0.09f};
        float Quadratic{0.032f};
        float AmbientIntensity{0.0f};
        float DiffuseIntensity{1.5f};
        float SpecularIntensity{1.5f};
    };

} // namespace Leon
