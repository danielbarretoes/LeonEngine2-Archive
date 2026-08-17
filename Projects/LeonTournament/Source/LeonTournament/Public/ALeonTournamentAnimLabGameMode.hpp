#pragma once

#include "ALeonTournamentGameMode.hpp"
#include "ALeonTournamentDummy.hpp"

namespace Leon {

    /**
     * Anim lab: Blend2D locomotion, jump/fall, death + ragdoll, dummy combat.
     * Camera defaults to third person; V toggles first/third person.
     */
    class ALeonTournamentAnimLabGameMode : public ALeonTournamentGameMode {
    public:
        ALeonTournamentAnimLabGameMode() = default;
        ALeonTournamentAnimLabGameMode(entt::entity InHandle, UWorld* InWorld,
                                       const std::string& InName = "LeonTournamentAnimLabGameMode");

        void InitGame() override;
        void StartPlay() override;
        void Tick(float DeltaSeconds) override;
        void RestartPlayer(AController* NewPlayer) override;

        ALeonTournamentDummy* GetDummy() const { return Dummy; }
        bool PrefersThirdPerson() const { return bPreferThirdPerson; }
        void SetPreferThirdPerson(bool bEnabled) { bPreferThirdPerson = bEnabled; }

    private:
        void ApplyLabClasses();
        void BuildAnimLab();
        void SpawnDummy();
        void RespawnDummy();
        void ApplyCameraPreference(ALeonTournamentCharacter* InCharacter);
        void HandleCameraToggle();
        void TickDummyRespawn(float DeltaSeconds);

        ALeonTournamentDummy* Dummy = nullptr;
        float DummyRespawnRemaining = -1.0f;
        bool bPreferThirdPerson = true;
        bool bCameraToggleWasDown = false;
    };

} // namespace Leon
