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

    private:
        void Explode(const glm::vec3& InLocation, const glm::vec3& InNormal, ALeonTournamentCharacter* InDirectHit);
        void AttachVisual();
        void SpawnTrail();

        ALeonTournamentCharacter* InstigatorCharacter = nullptr;
        ALeonTournamentWeapon* CauserWeapon = nullptr;
        FLeonTournamentWeaponConfig Config;
        float TrailCooldown = 0.0f;
        glm::vec3 VisualColor{0.95f, 0.35f, 0.08f};
    };

} // namespace Leon
