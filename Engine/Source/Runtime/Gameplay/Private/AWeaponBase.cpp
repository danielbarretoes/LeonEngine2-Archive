#include "Gameplay/AWeaponBase.hpp"
#include "Engine/UWorld.hpp"
#include "Engine/ENetTypes.hpp"

#include <algorithm>

namespace Leon {

    AWeaponBase::AWeaponBase(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AActor(InHandle, InWorld, InName) {
        SetClass("AWeaponBase");
    }

    bool AWeaponBase::IsClientNetMode() const {
        return World && World->GetNetMode() == ENetMode::Client;
    }

    void AWeaponBase::SetAmmoCapacity(int32_t InCount) {
        AmmoCapacity = std::max(InCount, 0);
        if (CurrentAmmo > AmmoCapacity)
            CurrentAmmo = AmmoCapacity;
    }

    void AWeaponBase::SetFireRate(float InShotsPerSecond) {
        FireRate = std::max(InShotsPerSecond, 0.0f);
    }

    void AWeaponBase::SetReloadTime(float InSeconds) {
        ReloadTime = std::max(InSeconds, 0.0f);
    }

    bool AWeaponBase::CanFire() const {
        return OwnerPawn && !bReloading && CurrentAmmo > 0 && FireCooldown <= 0.0f;
    }

    bool AWeaponBase::ServerFire() {
        if (IsClientNetMode() || !CanFire())
            return false;
        --CurrentAmmo;
        FireCooldown = FireRate > 0.0f ? 1.0f / FireRate : 0.1f;
        if (CurrentAmmo <= 0)
            StartReload();
        return true;
    }

    bool AWeaponBase::StartReload() {
        if (IsClientNetMode() || bReloading || CurrentAmmo >= AmmoCapacity)
            return false;
        bReloading = true;
        ReloadRemaining = ReloadTime;
        return true;
    }

    void AWeaponBase::CancelReload() {
        bReloading = false;
        ReloadRemaining = 0.0f;
    }

    void AWeaponBase::ResetMagazine() {
        CancelReload();
        CurrentAmmo = AmmoCapacity;
        FireCooldown = 0.0f;
    }

    void AWeaponBase::Tick(float DeltaSeconds) {
        AActor::Tick(DeltaSeconds);
        if (FireCooldown > 0.0f)
            FireCooldown = std::max(0.0f, FireCooldown - DeltaSeconds);
        if (IsClientNetMode())
            return;
        if (CurrentAmmo <= 0 && !bReloading)
            StartReload();
        if (!bReloading)
            return;
        ReloadRemaining -= DeltaSeconds;
        if (ReloadRemaining > 0.0f)
            return;
        CurrentAmmo = AmmoCapacity;
        bReloading = false;
        ReloadRemaining = 0.0f;
    }

} // namespace Leon
