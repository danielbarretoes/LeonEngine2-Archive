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

        AGameState* GetMatchGameState() const;

        virtual void StartMatch();
        virtual void EndMatch();

        bool HasMatchStarted() const;
        bool HasMatchEnded() const;
    };

} // namespace Leon
