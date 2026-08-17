#pragma once

#include "Gameplay/UCombatComponent.hpp"

namespace Leon {

    class AShooterWeapon;
    class AShooterCharacter;

    class UShooterCombatComponent : public UCombatComponent {
    public:
        UShooterCombatComponent(const std::string& InName = "ShooterCombat");

        void SetWeapon(AShooterWeapon* InWeapon);
        AShooterWeapon* GetWeapon() const { return Weapon; }

        void SetFireHeld(bool bHeld);
        void RequestReload();
        bool CanFireWeapon() const;

        AShooterCharacter* GetShooterOwner() const;

    private:
        AShooterWeapon* Weapon = nullptr;
    };

} // namespace Leon
