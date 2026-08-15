#pragma once

#include "gameplay/AActor.hpp"

namespace Leon {

    /**
     * @brief Unreal Engine aligned PlayerState holding replicated/persistent player data.
     */
    class APlayerState : public AActor {
    public:
        APlayerState() = default;
        APlayerState(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "PlayerState");
        ~APlayerState() override = default;

        const std::string& GetPlayerName() const { return m_PlayerName; }
        void SetPlayerName(const std::string& InName) { m_PlayerName = InName; }

        int32_t GetPlayerId() const { return m_PlayerId; }
        void SetPlayerId(int32_t InId) { m_PlayerId = InId; }

        float GetScore() const { return m_Score; }
        void SetScore(float InScore) { m_Score = InScore; }

    private:
        std::string m_PlayerName = "Player";
        int32_t m_PlayerId = 0;
        float m_Score = 0.0f;
    };

} // namespace Leon
