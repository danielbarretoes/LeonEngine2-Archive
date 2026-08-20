#include "Engine/FParticleSimulation.hpp"

#include <algorithm>
#include <cmath>

namespace Leon {

    float ParticleRng01(uint32_t& InOutState) {
        InOutState = InOutState * 1664525u + 1013904223u;
        return static_cast<float>(InOutState & 0x00FFFFFFu) / static_cast<float>(0x00FFFFFFu);
    }

    glm::vec3 ParticleRandomInRange(uint32_t& InOutState, const glm::vec3& InMin, const glm::vec3& InMax) {
        return {InMin.x + (InMax.x - InMin.x) * ParticleRng01(InOutState),
                InMin.y + (InMax.y - InMin.y) * ParticleRng01(InOutState),
                InMin.z + (InMax.z - InMin.z) * ParticleRng01(InOutState)};
    }

    int32_t AppendBurstParticles(std::vector<FSimulatedParticle>& InOutParticles,
                                 const FParticleEmitterSettings& InSettings, const glm::vec3& InOrigin,
                                 uint32_t& InOutRngState, int32_t InMaxParticles) {
        const int32_t room = std::max(0, InMaxParticles - static_cast<int32_t>(InOutParticles.size()));
        if (room <= 0)
            return 0;
        const int32_t spawnCount = std::min(std::clamp(InSettings.BurstCount, 1, kMaxBurstParticles), room);
        InOutParticles.reserve(InOutParticles.size() + static_cast<size_t>(spawnCount));
        for (int32_t i = 0; i < spawnCount; ++i) {
            FSimulatedParticle p;
            p.Kind = EParticleKind::SpriteBurst;
            p.Location = InOrigin;
            p.Velocity = ParticleRandomInRange(InOutRngState, InSettings.VelocityMin, InSettings.VelocityMax);
            p.Color = InSettings.Color;
            p.ColorEnd = InSettings.ColorEnd;
            p.Lifetime = std::max(0.01f, InSettings.Lifetime);
            p.Size = InSettings.Size;
            p.SizeEnd = InSettings.SizeEnd;
            p.Gravity = InSettings.Gravity;
            InOutParticles.push_back(p);
        }
        return spawnCount;
    }

    bool AppendBeamParticle(std::vector<FSimulatedParticle>& InOutParticles, const FParticleEmitterSettings& InSettings,
                            const glm::vec3& InStart, int32_t InMaxParticles) {
        if (static_cast<int32_t>(InOutParticles.size()) >= InMaxParticles)
            return false;
        FSimulatedParticle p;
        p.Kind = EParticleKind::Beam;
        p.Location = InStart;
        p.BeamEnd = InSettings.BeamEnd;
        p.Color = InSettings.Color;
        p.ColorEnd = InSettings.ColorEnd;
        p.Lifetime = std::max(0.01f, InSettings.Lifetime);
        p.Size = InSettings.BeamThickness;
        p.SizeEnd = InSettings.BeamThickness * 0.35f;
        InOutParticles.push_back(p);
        return true;
    }

    void SimulateParticles(std::vector<FSimulatedParticle>& InOutParticles, float InDeltaSeconds) {
        for (FSimulatedParticle& p : InOutParticles) {
            p.Age += InDeltaSeconds;
            if (p.Kind == EParticleKind::SpriteBurst) {
                p.Velocity += p.Gravity * InDeltaSeconds;
                p.Location += p.Velocity * InDeltaSeconds;
            }
        }
        InOutParticles.erase(std::remove_if(InOutParticles.begin(), InOutParticles.end(),
                                            [](const FSimulatedParticle& p) { return p.Age >= p.Lifetime; }),
                             InOutParticles.end());
    }

    void CopyParticlesForRender(const std::vector<FSimulatedParticle>& InParticles,
                                std::vector<FParticleInstance>& OutRenderParticles) {
        OutRenderParticles.clear();
        OutRenderParticles.reserve(InParticles.size());
        for (const FSimulatedParticle& p : InParticles)
            OutRenderParticles.push_back(p);
    }

} // namespace Leon
