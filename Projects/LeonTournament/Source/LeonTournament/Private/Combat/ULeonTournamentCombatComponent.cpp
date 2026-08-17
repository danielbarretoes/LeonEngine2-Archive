#include "ULeonTournamentCombatComponent.hpp"
#include "ALeonTournamentWeapon.hpp"
#include "ALeonTournamentCharacter.hpp"

namespace Leon {

    ULeonTournamentCombatComponent::ULeonTournamentCombatComponent(const std::string& InName) : UCombatComponent(InName) {}

    ALeonTournamentCharacter* ULeonTournamentCombatComponent::GetCharacterOwner() const {
        return dynamic_cast<ALeonTournamentCharacter*>(GetOwner());
    }

    void ULeonTournamentCombatComponent::SetWeapon(ALeonTournamentWeapon* InWeapon) {
        Weapon = InWeapon;
    }

    void ULeonTournamentCombatComponent::SetFireHeld(bool bHeld) {
        if (Weapon)
            Weapon->SetFireHeld(bHeld);
    }

    void ULeonTournamentCombatComponent::RequestReload() {
        if (Weapon)
            Weapon->StartReload();
    }

    bool ULeonTournamentCombatComponent::CanFireWeapon() const {
        return Weapon && Weapon->CanFire();
    }

} // namespace Leon
