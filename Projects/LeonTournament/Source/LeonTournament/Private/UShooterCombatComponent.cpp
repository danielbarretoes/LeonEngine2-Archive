#include "UShooterCombatComponent.hpp"
#include "AShooterWeapon.hpp"
#include "AShooterCharacter.hpp"

namespace Leon {

    UShooterCombatComponent::UShooterCombatComponent(const std::string& InName) : UCombatComponent(InName) {}

    AShooterCharacter* UShooterCombatComponent::GetShooterOwner() const {
        return dynamic_cast<AShooterCharacter*>(GetOwner());
    }

    void UShooterCombatComponent::SetWeapon(AShooterWeapon* InWeapon) {
        Weapon = InWeapon;
    }

    void UShooterCombatComponent::SetFireHeld(bool bHeld) {
        if (Weapon)
            Weapon->SetFireHeld(bHeld);
    }

    void UShooterCombatComponent::RequestReload() {
        if (Weapon)
            Weapon->StartReload();
    }

    bool UShooterCombatComponent::CanFireWeapon() const {
        return Weapon && Weapon->CanFire();
    }

} // namespace Leon
