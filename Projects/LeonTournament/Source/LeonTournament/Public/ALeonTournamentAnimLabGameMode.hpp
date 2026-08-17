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

    private:
        void ApplyLabClasses();
        void BuildAnimLab();
        void SpawnDummy();
        void RespawnDummy();
        void TickDummyRespawn(float DeltaSeconds);
        void EnsureLabColliders();

        ALeonTournamentDummy* Dummy = nullptr;
        float DummyRespawnRemaining = -1.0f;
    };

} // namespace Leon
