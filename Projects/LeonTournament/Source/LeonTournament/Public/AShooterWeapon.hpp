#pragma once

#include "Gameplay/AActor.hpp"
#include "FShooterTypes.hpp"

namespace Leon {

    class AShooterCharacter;

    class AShooterWeapon : public AActor {
    public:
        AShooterWeapon() = default;
        AShooterWeapon(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "ShooterWeapon");

        void Tick(float DeltaSeconds) override;

        void SetOwnerCharacter(AShooterCharacter* InOwner) { OwnerCharacter = InOwner; }
        AShooterCharacter* GetOwnerCharacter() const { return OwnerCharacter; }

        const FShooterRifleConfig& GetConfig() const { return Config; }
        void SetConfig(const FShooterRifleConfig& InConfig);

        int32_t GetCurrentAmmo() const { return CurrentAmmo; }
        int32_t GetMagazineSize() const { return Config.MagazineSize; }
        bool IsReloading() const { return bReloading; }
        bool IsFiring() const { return bFiring; }

        void SetFireHeld(bool bHeld) { bFireHeld = bHeld; }
        bool CanFire() const;
        bool ServerFire();
        bool StartReload();
        void CancelReload();
        void ResetMagazine();
        void ApplyReplicatedState(int32_t InAmmo, bool bInReloading);
        float GetReloadRemaining() const { return ReloadRemaining; }

        void AttachVisual();

    protected:
        FShooterRifleConfig Config;
        AShooterCharacter* OwnerCharacter = nullptr;
        int32_t CurrentAmmo = 30;
        float FireCooldown = 0.0f;
        float ReloadRemaining = 0.0f;
        bool bReloading = false;
        bool bFireHeld = false;
        bool bFiring = false;
        bool bVisualReady = false;
    };

    class AShooterRifle : public AShooterWeapon {
    public:
        AShooterRifle() = default;
        AShooterRifle(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "ShooterRifle");
    };

} // namespace Leon
