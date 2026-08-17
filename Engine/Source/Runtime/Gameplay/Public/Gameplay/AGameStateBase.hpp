#pragma once

#include "Gameplay/AActor.hpp"
#include "Gameplay/APlayerState.hpp"
#include <vector>

namespace Leon {

    /**
     * @brief Unreal Engine aligned GameStateBase holding global game/match state.
     */
    class AGameStateBase : public AActor {
    public:
        AGameStateBase() = default;
        AGameStateBase(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "GameStateBase");
        ~AGameStateBase() override = default;

        void AddPlayerState(APlayerState* InPlayerState);
        void RemovePlayerState(APlayerState* InPlayerState);
        void ClearPlayerArray() { PlayerArray.clear(); }
        const std::vector<APlayerState*>& GetPlayerArray() const { return PlayerArray; }

        float GetElapsedTime() const { return ElapsedTime; }
        void SetElapsedTime(float InTime) { ElapsedTime = InTime; }
        void Tick(float DeltaSeconds) override;

    private:
        std::vector<APlayerState*> PlayerArray;
        float ElapsedTime = 0.0f;
    };

} // namespace Leon
