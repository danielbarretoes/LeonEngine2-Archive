#pragma once

#include "Gameplay/AActor.hpp"
#include "FLeonTournamentTypes.hpp"

namespace Leon {

    class ALeonTournamentCharacter;

    class ALeonTournamentWeapon : public AActor {
    public:
        ALeonTournamentWeapon() = default;
        ALeonTournamentWeapon(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "LeonTournamentWeapon");

        void Tick(float DeltaSeconds) override;

        void SetOwnerCharacter(ALeonTournamentCharacter* InOwner) { OwnerCharacter = InOwner; }
        ALeonTournamentCharacter* GetOwnerCharacter() const { return OwnerCharacter; }

        const FLeonTournamentRifleConfig& GetConfig() const { return Config; }
        void SetConfig(const FLeonTournamentRifleConfig& InConfig);

        int32_t GetCurrentAmmo() const { return CurrentAmmo; }
        int32_t GetMagazineSize() const { return Config.MagazineSize; }
        bool IsReloading() const { return bReloading; }
        bool IsFiring() const { return bFiring; }

        void SetFireHeld(bool bHeld);
        bool CanFire() const;
        bool ServerFire();
        bool StartReload();
        void CancelReload();
        void ResetMagazine();
        void ApplyReplicatedState(int32_t InAmmo, bool bInReloading);
        float GetReloadRemaining() const { return ReloadRemaining; }

        /** True when magazine is empty and a reload can start. */
        bool NeedsReload() const;

        void AttachVisual();
        void SetVisualHidden(bool bHidden);
        glm::vec3 GetMuzzleLocation() const;
        int32_t GetLastVfxSpawnCount() const { return LastVfxSpawnCount; }

    private:
        void SpawnFireEffects(const glm::vec3& InMuzzle, const glm::vec3& InTraceEnd, bool bHitWorld,
                              bool bHitCharacter);
        void UpdateFirstPersonVisual();

    protected:
        FLeonTournamentRifleConfig Config;
        ALeonTournamentCharacter* OwnerCharacter = nullptr;
        int32_t CurrentAmmo = 30;
        float FireCooldown = 0.0f;
        float ReloadRemaining = 0.0f;
        bool bReloading = false;
        bool bFireHeld = false;
        bool bFiring = false;
        bool bVisualReady = false;
        bool bVisualHidden = false;
        int32_t LastVfxSpawnCount = 0;
    };

    class ALeonTournamentRifle : public ALeonTournamentWeapon {
    public:
        ALeonTournamentRifle() = default;
        ALeonTournamentRifle(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "LeonTournamentRifle");
    };

} // namespace Leon
