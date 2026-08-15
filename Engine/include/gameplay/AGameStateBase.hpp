#pragma once

#include "gameplay/AActor.hpp"
#include "gameplay/APlayerState.hpp"
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
        const std::vector<APlayerState*>& GetPlayerArray() const { return m_PlayerArray; }

        float GetElapsedTime() const { return m_ElapsedTime; }
        void Tick(float DeltaSeconds) override;

    private:
        std::vector<APlayerState*> m_PlayerArray;
        float m_ElapsedTime = 0.0f;
    };

} // namespace Leon
