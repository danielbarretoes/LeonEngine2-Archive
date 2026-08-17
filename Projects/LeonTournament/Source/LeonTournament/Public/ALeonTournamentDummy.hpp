#pragma once

#include "ALeonTournamentCharacter.hpp"

namespace Leon {

    /**
     * Standing mannequin for the third-person anim lab: Health + Combat, no rifle.
     */
    class ALeonTournamentDummy : public ALeonTournamentCharacter {
    public:
        ALeonTournamentDummy() = default;
        ALeonTournamentDummy(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "LeonTournamentDummy");

        void PostInitializeComponents() override;
        void BeginPlay() override;
        void Tick(float DeltaSeconds) override;
        bool ShouldSpawnWeapon() const override { return false; }

    private:
        void PunchNearest();
        bool bCombatBound = false;
    };

} // namespace Leon
