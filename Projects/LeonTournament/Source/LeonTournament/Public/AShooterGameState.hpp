#pragma once

#include "Gameplay/AGameStateBase.hpp"
#include "FShooterTypes.hpp"
#include "Engine/FNetBlob.hpp"

#include <vector>

namespace Leon {

    class AShooterPlayerState;

    class AShooterGameState : public AGameStateBase {
    public:
        AShooterGameState() = default;
        AShooterGameState(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "ShooterGameState");

        EShooterMatchState GetMatchState() const { return MatchState; }
        void SetMatchState(EShooterMatchState InState) { MatchState = InState; }

        float GetRemainingTime() const { return RemainingTime; }
        void SetRemainingTime(float InTime) { RemainingTime = InTime; }

        int32_t GetTeam1Kills() const { return Team1Kills; }
        int32_t GetTeam2Kills() const { return Team2Kills; }
        void SetTeam1Kills(int32_t InKills) { Team1Kills = InKills; }
        void SetTeam2Kills(int32_t InKills) { Team2Kills = InKills; }
        void AddTeamKill(EShooterTeam InTeam);

        int32_t GetTeam1PlayerCount() const { return Team1PlayerCount; }
        int32_t GetTeam2PlayerCount() const { return Team2PlayerCount; }
        void SetTeam1PlayerCount(int32_t InCount) { Team1PlayerCount = InCount; }
        void SetTeam2PlayerCount(int32_t InCount) { Team2PlayerCount = InCount; }

        EShooterMatchWinner GetMatchWinner() const { return MatchWinner; }
        void SetMatchWinner(EShooterMatchWinner InWinner) { MatchWinner = InWinner; }

        float GetCountdownRemaining() const { return CountdownRemaining; }
        void SetCountdownRemaining(float InTime) { CountdownRemaining = InTime; }

        std::vector<AShooterPlayerState*> GetSortedScoreboard() const;

        void SerializeReplication(std::vector<uint8_t>& OutBytes) const override;
        void DeserializeReplication(const uint8_t* InData, size_t InSize) override;

    private:
        EShooterMatchState MatchState = EShooterMatchState::MainMenu;
        float RemainingTime = 600.0f;
        int32_t Team1Kills = 0;
        int32_t Team2Kills = 0;
        int32_t Team1PlayerCount = 0;
        int32_t Team2PlayerCount = 0;
        EShooterMatchWinner MatchWinner = EShooterMatchWinner::None;
        float CountdownRemaining = 0.0f;
    };

} // namespace Leon
