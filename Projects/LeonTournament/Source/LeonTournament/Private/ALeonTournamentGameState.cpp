#include "ALeonTournamentGameState.hpp"
#include "ALeonTournamentPlayerState.hpp"

#include <algorithm>

namespace Leon {

    ALeonTournamentGameState::ALeonTournamentGameState(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AGameStateBase(InHandle, InWorld, InName) {
        SetClass("ALeonTournamentGameState");
    }

    void ALeonTournamentGameState::AddTeamKill(ELeonTournamentTeam InTeam) {
        if (InTeam == ELeonTournamentTeam::Team1)
            ++Team1Kills;
        else if (InTeam == ELeonTournamentTeam::Team2)
            ++Team2Kills;
    }

    std::vector<ALeonTournamentPlayerState*> ALeonTournamentGameState::GetSortedScoreboard() const {
        std::vector<ALeonTournamentPlayerState*> rows;
        for (APlayerState* ps : GetPlayerArray()) {
            if (auto* sps = dynamic_cast<ALeonTournamentPlayerState*>(ps))
                rows.push_back(sps);
        }
        std::sort(rows.begin(), rows.end(), [](ALeonTournamentPlayerState* a, ALeonTournamentPlayerState* b) {
            if (a->GetKills() != b->GetKills())
                return a->GetKills() > b->GetKills();
            return a->GetDeaths() < b->GetDeaths();
        });
        return rows;
    }

    void ALeonTournamentGameState::SerializeReplication(std::vector<uint8_t>& OutBytes) const {
        FNetBlob::WriteU8(OutBytes, static_cast<uint8_t>(MatchState));
        FNetBlob::WriteF32(OutBytes, RemainingTime);
        FNetBlob::WriteF32(OutBytes, CountdownRemaining);
        FNetBlob::WriteI32(OutBytes, Team1Kills);
        FNetBlob::WriteI32(OutBytes, Team2Kills);
        FNetBlob::WriteI32(OutBytes, Team1PlayerCount);
        FNetBlob::WriteI32(OutBytes, Team2PlayerCount);
        FNetBlob::WriteU8(OutBytes, static_cast<uint8_t>(MatchWinner));
    }

    void ALeonTournamentGameState::DeserializeReplication(const uint8_t* InData, size_t InSize) {
        std::vector<uint8_t> bytes(InData, InData + InSize);
        size_t offset = 0;
        uint8_t state = 0, winner = 0;
        if (!FNetBlob::ReadU8(bytes, offset, state) || !FNetBlob::ReadF32(bytes, offset, RemainingTime) ||
            !FNetBlob::ReadF32(bytes, offset, CountdownRemaining) || !FNetBlob::ReadI32(bytes, offset, Team1Kills) ||
            !FNetBlob::ReadI32(bytes, offset, Team2Kills) || !FNetBlob::ReadI32(bytes, offset, Team1PlayerCount) ||
            !FNetBlob::ReadI32(bytes, offset, Team2PlayerCount) || !FNetBlob::ReadU8(bytes, offset, winner))
            return;
        MatchState = static_cast<ELeonTournamentMatchState>(state);
        MatchWinner = static_cast<ELeonTournamentMatchWinner>(winner);
    }

} // namespace Leon
