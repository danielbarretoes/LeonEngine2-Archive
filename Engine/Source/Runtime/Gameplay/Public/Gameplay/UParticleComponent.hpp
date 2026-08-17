#pragma once

#include "Gameplay/UActorComponent.hpp"
#include "Engine/FParticleTypes.hpp"

namespace Leon {

    /**
     * Runtime particle emitter. Engine-generic: burst sprites and short beams.
     */
    class UParticleComponent : public UActorComponent {
    public:
        UParticleComponent(const std::string& InName = "ParticleComponent");

        void BeginPlay() override;
        void Tick(float DeltaSeconds) override;

        void SetEmitterSettings(const FParticleEmitterSettings& InSettings) { Settings = InSettings; }
        const FParticleEmitterSettings& GetEmitterSettings() const { return Settings; }

        void Activate(bool bReset = true);
        void Deactivate();
        bool IsActive() const { return bActive; }

        int32_t GetParticleCount() const { return static_cast<int32_t>(Particles.size()); }
        const std::vector<FParticleInstance>& GetParticles() const { return Particles; }

        void SetDestroyOwnerWhenDone(bool bDestroy) { bDestroyOwnerWhenDone = bDestroy; }

    private:
        void SpawnBurst(const glm::vec3& InOrigin);
        void SpawnBeam(const glm::vec3& InStart, const glm::vec3& InEnd);
        void PushToRenderComponent();
        glm::vec3 RandomInRange(const glm::vec3& InMin, const glm::vec3& InMax);

        FParticleEmitterSettings Settings;
        std::vector<FParticleInstance> Particles;
        uint32_t RngState = 1;
        bool bActive = false;
        bool bHasEmitted = false;
        bool bDestroyOwnerWhenDone = true;
    };

} // namespace Leon
