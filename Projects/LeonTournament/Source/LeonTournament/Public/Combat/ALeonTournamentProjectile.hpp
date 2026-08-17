#pragma once

#include "Gameplay/AProjectile.hpp"
#include "FLeonTournamentTypes.hpp"

namespace Leon {

    class ALeonTournamentCharacter;
    class ALeonTournamentWeapon;

    class ALeonTournamentProjectile : public AProjectile {
    public:
        ALeonTournamentProjectile() = default;
        ALeonTournamentProjectile(entt::entity InHandle, UWorld* InWorld,
                                  const std::string& InName = "LeonTournamentProjectile");

        void BeginPlay() override;
        void Tick(float DeltaSeconds) override;
        void NotifyHit(const FHitResult& InHit) override;
        void OnLifeSpanExpired() override;

        void Launch(ALeonTournamentCharacter* InInstigator, ALeonTournamentWeapon* InCauser, const glm::vec3& InDir,
                    const FLeonTournamentWeaponConfig& InConfig);

        /** Shotgun pellets: point damage, visible trails, optional surface bounce. */
        bool IsPellet() const { return Config.SplashRadius < 0.05f; }

    private:
        void Explode(const glm::vec3& InLocation, const glm::vec3& InNormal, ALeonTournamentCharacter* InDirectHit);
        void ImpactPellet(const glm::vec3& InLocation, const glm::vec3& InNormal, ALeonTournamentCharacter* InDirectHit);
        void SpawnRicochetFx(const glm::vec3& InLocation, const glm::vec3& InNormal, const glm::vec3& InOutDir);
        void AttachVisual();
        void SpawnTrail();
        void ApplyPointDamage(ALeonTournamentCharacter* InTarget, const glm::vec3& InLocation,
                              const glm::vec3& InNormal, const glm::vec3& InImpulseDir);

        ALeonTournamentCharacter* InstigatorCharacter = nullptr;
        ALeonTournamentWeapon* CauserWeapon = nullptr;
        FLeonTournamentWeaponConfig Config;
        float TrailCooldown = 0.0f;
        int32_t RemainingBounces = 0;
        glm::vec3 VisualColor{0.95f, 0.35f, 0.08f};
    };

} // namespace Leon
