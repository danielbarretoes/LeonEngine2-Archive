#pragma once

#include "Gameplay/AGameStateBase.hpp"
#include "Gameplay/EMatchState.hpp"

namespace Leon {

    /**
     * Match-aware GameState: phase and remaining clock, replicated to clients.
     */
    class AGameState : public AGameStateBase {
    public:
        AGameState() = default;
        AGameState(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "GameState");
        ~AGameState() override = default;

        void Tick(float DeltaSeconds) override;

        EMatchState GetMatchState() const { return MatchState; }
        void SetMatchState(EMatchState InState);
        virtual void HandleMatchStateChange(EMatchState InPrevious, EMatchState InCurrent);

        float GetRemainingTime() const { return RemainingTime; }
        void SetRemainingTime(float InTime);

        void SerializeReplication(std::vector<uint8_t>& OutBytes) const override;
        void DeserializeReplication(const uint8_t* InData, size_t InSize) override;

    private:
        EMatchState MatchState = EMatchState::WaitingToStart;
        float RemainingTime = 0.0f;
    };

} // namespace Leon
