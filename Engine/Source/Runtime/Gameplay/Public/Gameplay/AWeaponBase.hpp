#pragma once

#include "Gameplay/AActor.hpp"

#include <cstdint>

namespace Leon {

    class APawn;

    /**
     * Minimal held-item actor: owner pawn, magazine, fire cooldown, reload timer.
     * Subclasses implement traces, VFX, and product fire rules. No teams or scoring.
     */
    class AWeaponBase : public AActor {
    public:
        AWeaponBase() = default;
        AWeaponBase(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "WeaponBase");

        void Tick(float DeltaSeconds) override;

        void SetOwnerPawn(APawn* InPawn) { OwnerPawn = InPawn; }
        APawn* GetOwnerPawn() const { return OwnerPawn; }

        int32_t GetCurrentAmmo() const { return CurrentAmmo; }
        int32_t GetAmmoCapacity() const { return AmmoCapacity; }
        void SetAmmoCapacity(int32_t InCount);
        float GetFireRate() const { return FireRate; }
        void SetFireRate(float InShotsPerSecond);
        float GetReloadTime() const { return ReloadTime; }
        void SetReloadTime(float InSeconds);
        bool IsReloadInProgress() const { return bReloading; }
        float GetReloadRemaining() const { return ReloadRemaining; }

        virtual bool CanFire() const;
        virtual bool ServerFire();
        virtual bool StartReload();
        virtual void ResetMagazine();
        void CancelReload();

    protected:
        bool IsClientNetMode() const;

        APawn* OwnerPawn = nullptr;
        int32_t AmmoCapacity = 0;
        int32_t CurrentAmmo = 0;
        float FireRate = 10.0f;
        float ReloadTime = 1.0f;
        float FireCooldown = 0.0f;
        float ReloadRemaining = 0.0f;
        bool bReloading = false;
    };

} // namespace Leon
