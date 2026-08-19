#pragma once

#include "Gameplay/AActor.hpp"

namespace Leon {

    class APlayerController;

    /**
     * @brief Unreal Engine aligned PlayerState holding replicated/persistent player data.
     */
    class APlayerState : public AActor {
    public:
        APlayerState() = default;
        APlayerState(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "PlayerState");
        ~APlayerState() override = default;

        APlayerController* GetPlayerController() const;

        const std::string& GetPlayerName() const { return PlayerName; }
        void SetPlayerName(const std::string& InName) { PlayerName = InName; }

        int32_t GetPlayerId() const { return PlayerId; }
        void SetPlayerId(int32_t InId) { PlayerId = InId; }

        float GetScore() const { return Score; }
        /** Authority and snapshot apply (UNetDriver) write Score; HUD ranks by this field. */
        void SetScore(float InScore) { Score = InScore; }

    private:
        std::string PlayerName = "Player";
        int32_t PlayerId = 0;
        float Score = 0.0f;
    };

} // namespace Leon
