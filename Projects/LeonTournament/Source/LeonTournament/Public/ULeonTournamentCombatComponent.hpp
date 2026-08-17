#pragma once

#include "Gameplay/UCombatComponent.hpp"

namespace Leon {

    class ALeonTournamentWeapon;
    class ALeonTournamentCharacter;

    class ULeonTournamentCombatComponent : public UCombatComponent {
    public:
        ULeonTournamentCombatComponent(const std::string& InName = "LeonTournamentCombat");

        void SetWeapon(ALeonTournamentWeapon* InWeapon);
        ALeonTournamentWeapon* GetWeapon() const { return Weapon; }

        void SetFireHeld(bool bHeld);
        void RequestReload();
        bool CanFireWeapon() const;

        ALeonTournamentCharacter* GetCharacterOwner() const;

    private:
        ALeonTournamentWeapon* Weapon = nullptr;
    };

} // namespace Leon
