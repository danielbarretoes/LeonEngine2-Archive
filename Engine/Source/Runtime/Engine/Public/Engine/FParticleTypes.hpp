#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <vector>

namespace Leon {

    enum class EParticleKind : uint8_t { SpriteBurst = 0, Beam = 1 };

    /**
     * Generic one-shot / short-lived emitter description.
     * Gameplay meaning (muzzle, tracer, impact) belongs in game code.
     */
    struct FParticleEmitterSettings {
        EParticleKind Kind = EParticleKind::SpriteBurst;
        int32_t BurstCount = 10;
        float Lifetime = 0.08f;
        float Size = 0.06f;
        float SizeEnd = 0.015f;
        glm::vec4 Color{1.0f, 1.0f, 1.0f, 1.0f};
        glm::vec4 ColorEnd{1.0f, 1.0f, 1.0f, 0.0f};
        glm::vec3 VelocityMin{-0.5f, 0.1f, -0.5f};
        glm::vec3 VelocityMax{0.5f, 1.4f, 0.5f};
        glm::vec3 Gravity{0.0f, -6.0f, 0.0f};
        glm::vec3 BeamEnd{0.0f};
        float BeamThickness = 0.025f;
        bool bOneShot = true;
    };

    struct FParticleInstance {
        glm::vec3 Location{0.0f};
        glm::vec3 Velocity{0.0f};
        glm::vec3 BeamEnd{0.0f};
        glm::vec4 Color{1.0f};
        glm::vec4 ColorEnd{1.0f, 1.0f, 1.0f, 0.0f};
        float Age = 0.0f;
        float Lifetime = 0.08f;
        float Size = 0.06f;
        float SizeEnd = 0.015f;
        EParticleKind Kind = EParticleKind::SpriteBurst;
    };

    /** EnTT render mirror written by UParticleComponent. */
    struct FParticleRenderComponent {
        std::vector<FParticleInstance> Particles;
        bool bVisible = true;
    };

} // namespace Leon
