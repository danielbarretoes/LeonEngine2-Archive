#pragma once

#include "Gameplay/AWeaponBase.hpp"
#include "FLeonTournamentTypes.hpp"

namespace Leon {

    class ALeonTournamentCharacter;

    class ALeonTournamentWeapon : public AWeaponBase {
    public:
        ALeonTournamentWeapon() = default;
        ALeonTournamentWeapon(entt::entity InHandle, UWorld* InWorld,
                              const std::string& InName = "LeonTournamentWeapon");

        void Tick(float DeltaSeconds) override;

        void SetOwnerCharacter(ALeonTournamentCharacter* InOwner);
        ALeonTournamentCharacter* GetOwnerCharacter() const { return OwnerCharacter; }

        ELeonTournamentWeaponId GetWeaponId() const { return WeaponId; }
        void SetWeaponId(ELeonTournamentWeaponId InId);

        const FLeonTournamentWeaponConfig& GetConfig() const { return Config; }
        void SetConfig(const FLeonTournamentWeaponConfig& InConfig);

        bool IsFiring() const { return bFiring; }
        bool IsReloading() const { return IsReloadInProgress(); }
        int32_t GetMagazineSize() const { return GetAmmoCapacity(); }
        bool CanAimDownSights() const { return Config.ScopeFOV > 1.0f; }

        void SetFireHeld(bool bHeld);
        bool CanFire() const override;
        bool ServerFire() override;
        bool StartReload() override;
        void ResetMagazine() override;
        void ApplyReplicatedState(int32_t InAmmo, bool bInReloading);

        /** True when magazine is empty and a reload can start. */
        bool NeedsReload() const;

        void AttachVisual();
        void SetVisualHidden(bool bHidden);
        glm::vec3 GetMuzzleLocation() const;
        int32_t GetLastVfxSpawnCount() const { return LastVfxSpawnCount; }

        /** Current half-angle spread in degrees (for HUD bloom + aim jitter). */
        float GetCurrentSpreadDeg() const { return CurrentSpreadDeg; }
        /** 0 = tight crosshair, 1 = fully open at MaxSpread. */
        float GetSpreadAlpha() const;

    private:
        void SpawnFireEffects(const glm::vec3& InMuzzle, const glm::vec3& InTracerStart, const glm::vec3& InTraceEnd,
                              bool bHitWorld, bool bHitCharacter);
        void SpawnLaserEffects(const glm::vec3& InMuzzle, const glm::vec3& InTraceEnd, bool bHitCharacter);
        void SpawnRocketLaunchEffects(const glm::vec3& InMuzzle);
        void SpawnShotgunBlastEffects(const glm::vec3& InMuzzle, const glm::vec3& InAimDir);
        void SpawnFlameEffects(const glm::vec3& InMuzzle, const glm::vec3& InDir);
        void UpdateFirstPersonVisual();
        glm::vec3 ApplyAimSpread(const glm::vec3& InForward, float InHalfAngleDeg) const;
        void AddShotBloom();
        bool FireHitscan();
        bool FireProjectile();
        bool FireFlame();
        bool ApplyHitscanDamage(const glm::vec3& InOrigin, const glm::vec3& InDir, float InDamage,
                                glm::vec3& OutTraceEnd, bool& OutHitWorld, bool& OutHitCharacter,
                                int32_t InMaxBounces = 0);

    protected:
        ELeonTournamentWeaponId WeaponId = ELeonTournamentWeaponId::Rifle;
        FLeonTournamentWeaponConfig Config;
        ALeonTournamentCharacter* OwnerCharacter = nullptr;
        float CurrentSpreadDeg = 0.35f;
        bool bFireHeld = false;
        bool bFiring = false;
        bool bVisualReady = false;
        bool bVisualHidden = false;
        int32_t LastVfxSpawnCount = 0;
        float SwayPhase = 0.0f;
    };

    class ALeonTournamentRifle : public ALeonTournamentWeapon {
    public:
        ALeonTournamentRifle() = default;
        ALeonTournamentRifle(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "LeonTournamentRifle");
    };

    class ALeonTournamentShotgun : public ALeonTournamentWeapon {
    public:
        ALeonTournamentShotgun() = default;
        ALeonTournamentShotgun(entt::entity InHandle, UWorld* InWorld,
                               const std::string& InName = "LeonTournamentShotgun");
    };

    class ALeonTournamentRocketLauncher : public ALeonTournamentWeapon {
    public:
        ALeonTournamentRocketLauncher() = default;
        ALeonTournamentRocketLauncher(entt::entity InHandle, UWorld* InWorld,
                                      const std::string& InName = "LeonTournamentRocketLauncher");
    };

    class ALeonTournamentLaserRifle : public ALeonTournamentWeapon {
    public:
        ALeonTournamentLaserRifle() = default;
        ALeonTournamentLaserRifle(entt::entity InHandle, UWorld* InWorld,
                                  const std::string& InName = "LeonTournamentLaserRifle");
    };

    class ALeonTournamentGrenadeLauncher : public ALeonTournamentWeapon {
    public:
        ALeonTournamentGrenadeLauncher() = default;
        ALeonTournamentGrenadeLauncher(entt::entity InHandle, UWorld* InWorld,
                                       const std::string& InName = "LeonTournamentGrenadeLauncher");
    };

    class ALeonTournamentFlamethrower : public ALeonTournamentWeapon {
    public:
        ALeonTournamentFlamethrower() = default;
        ALeonTournamentFlamethrower(entt::entity InHandle, UWorld* InWorld,
                                    const std::string& InName = "LeonTournamentFlamethrower");
    };

} // namespace Leon
