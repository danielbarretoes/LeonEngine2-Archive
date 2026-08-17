#pragma once

#include "Gameplay/AGameStateBase.hpp"
#include "FLeonTournamentTypes.hpp"
#include "Engine/FNetBlob.hpp"

#include <vector>

namespace Leon {

    class ALeonTournamentPlayerState;

    class ALeonTournamentGameState : public AGameStateBase {
    public:
        ALeonTournamentGameState() = default;
        ALeonTournamentGameState(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "LeonTournamentGameState");

        ELeonTournamentMatchState GetMatchState() const { return MatchState; }
        void SetMatchState(ELeonTournamentMatchState InState) {
            if (!IsNetworkAuthority())
                return;
            MatchState = InState;
        }

        float GetRemainingTime() const { return RemainingTime; }
        void SetRemainingTime(float InTime) {
            if (!IsNetworkAuthority())
                return;
            RemainingTime = InTime;
        }

        int32_t GetTeam1Kills() const { return Team1Kills; }
        int32_t GetTeam2Kills() const { return Team2Kills; }
        void SetTeam1Kills(int32_t InKills) {
            if (!IsNetworkAuthority())
                return;
            Team1Kills = InKills;
        }
        void SetTeam2Kills(int32_t InKills) {
            if (!IsNetworkAuthority())
                return;
            Team2Kills = InKills;
        }
        void AddTeamKill(ELeonTournamentTeam InTeam);

        int32_t GetTeam1PlayerCount() const { return Team1PlayerCount; }
        int32_t GetTeam2PlayerCount() const { return Team2PlayerCount; }
        void SetTeam1PlayerCount(int32_t InCount) {
            if (!IsNetworkAuthority())
                return;
            Team1PlayerCount = InCount;
        }
        void SetTeam2PlayerCount(int32_t InCount) {
            if (!IsNetworkAuthority())
                return;
            Team2PlayerCount = InCount;
        }

        ELeonTournamentMatchWinner GetMatchWinner() const { return MatchWinner; }
        void SetMatchWinner(ELeonTournamentMatchWinner InWinner) {
            if (!IsNetworkAuthority())
                return;
            MatchWinner = InWinner;
        }

        float GetCountdownRemaining() const { return CountdownRemaining; }
        void SetCountdownRemaining(float InTime) {
            if (!IsNetworkAuthority())
                return;
            CountdownRemaining = InTime;
        }

        std::vector<ALeonTournamentPlayerState*> GetSortedScoreboard() const;

        void SerializeReplication(std::vector<uint8_t>& OutBytes) const override;
        void DeserializeReplication(const uint8_t* InData, size_t InSize) override;

    private:
        ELeonTournamentMatchState MatchState = ELeonTournamentMatchState::MainMenu;
        float RemainingTime = 600.0f;
        int32_t Team1Kills = 0;
        int32_t Team2Kills = 0;
        int32_t Team1PlayerCount = 0;
        int32_t Team2PlayerCount = 0;
        ELeonTournamentMatchWinner MatchWinner = ELeonTournamentMatchWinner::None;
        float CountdownRemaining = 0.0f;
    };

} // namespace Leon
