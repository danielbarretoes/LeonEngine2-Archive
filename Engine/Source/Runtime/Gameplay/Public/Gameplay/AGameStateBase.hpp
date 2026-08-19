#pragma once

#include "Gameplay/AActor.hpp"
#include "Gameplay/APlayerState.hpp"
#include <vector>

namespace Leon {

    class AGameModeBase;

    /**
     * @brief Unreal Engine aligned GameStateBase holding global game/match state.
     */
    class AGameStateBase : public AActor {
    public:
        AGameStateBase() = default;
        AGameStateBase(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "GameStateBase");
        ~AGameStateBase() override = default;

        AGameModeBase* GetGameMode() const;

        void AddPlayerState(APlayerState* InPlayerState);
        void RemovePlayerState(APlayerState* InPlayerState);
        void ClearPlayerArray() { PlayerArray.clear(); }
        const std::vector<APlayerState*>& GetPlayerArray() const { return PlayerArray; }
        /** Scoreboard query: PlayerArray ranked by Score (desc), then PlayerName. Safe on clients. */
        std::vector<APlayerState*> GetPlayerArraySortedByScore() const;

        float GetElapsedTime() const { return ElapsedTime; }
        void SetElapsedTime(float InTime) { ElapsedTime = InTime; }
        void Tick(float DeltaSeconds) override;

    protected:
        std::vector<APlayerState*> PlayerArray;
        float ElapsedTime = 0.0f;
    };

} // namespace Leon
