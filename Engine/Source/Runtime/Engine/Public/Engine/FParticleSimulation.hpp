#pragma once

#include "Engine/FParticleTypes.hpp"

#include <cstdint>
#include <vector>

namespace Leon {

    float ParticleRng01(uint32_t& InOutState);
    glm::vec3 ParticleRandomInRange(uint32_t& InOutState, const glm::vec3& InMin, const glm::vec3& InMax);

    int32_t AppendBurstParticles(std::vector<FSimulatedParticle>& InOutParticles, const FParticleEmitterSettings& InSettings,
                                 const glm::vec3& InOrigin, uint32_t& InOutRngState, int32_t InMaxParticles);

    bool AppendBeamParticle(std::vector<FSimulatedParticle>& InOutParticles, const FParticleEmitterSettings& InSettings,
                            const glm::vec3& InStart, int32_t InMaxParticles);

    void SimulateParticles(std::vector<FSimulatedParticle>& InOutParticles, float InDeltaSeconds);

    void CopyParticlesForRender(const std::vector<FSimulatedParticle>& InParticles,
                                std::vector<FParticleInstance>& OutRenderParticles);

} // namespace Leon
