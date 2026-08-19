#pragma once

#include "Gameplay/AGameModeBase.hpp"
#include "Gameplay/AGameState.hpp"

namespace Leon {

    /**
     * Match-aware GameMode: StartMatch / EndMatch write AGameState phase.
     * Login, spawn, and RestartPlayer stay on AGameModeBase.
     */
    class AGameMode : public AGameModeBase {
    public:
        AGameMode() = default;
        AGameMode(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "GameMode");
        ~AGameMode() override = default;

        AGameState* GetGameState() const;

        virtual void SetMatchState(EMatchState InState);
        virtual void StartMatch();
        virtual void EndMatch();
        virtual void RestartGame();

        virtual void HandleMatchIsWaitingToStart();
        virtual void HandleMatchHasStarted();
        virtual void HandleMatchHasEnded();

        bool PlayerCanRestart(AController* InPlayer) const override;
        bool HasMatchStarted() const;
        bool HasMatchEnded() const;
        bool HasMatchInProgress() const;
    };

} // namespace Leon
