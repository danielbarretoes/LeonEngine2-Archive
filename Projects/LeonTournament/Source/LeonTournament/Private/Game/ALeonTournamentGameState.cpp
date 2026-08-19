#include "ALeonTournamentGameState.hpp"
#include "ALeonTournamentPlayerState.hpp"
#include "Gameplay/AGameMode.hpp"
#include "Gameplay/EMatchState.hpp"

#include <algorithm>

namespace Leon {

    ALeonTournamentGameState::ALeonTournamentGameState(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AGameState(InHandle, InWorld, InName) {
        SetClass("ALeonTournamentGameState");
    }

    void ALeonTournamentGameState::SetMatchState(ELeonTournamentMatchState InState) {
        if (!IsNetworkAuthority())
            return;
        MatchState = InState;
        if (auto* gm = dynamic_cast<AGameMode*>(GetGameMode())) {
            if (InState == ELeonTournamentMatchState::Playing)
                gm->SetMatchState(EMatchState::InProgress);
            else if (InState == ELeonTournamentMatchState::Finished)
                gm->SetMatchState(EMatchState::WaitingPostMatch);
            else
                gm->SetMatchState(EMatchState::WaitingToStart);
        }
    }

    void ALeonTournamentGameState::AddTeamKill(ELeonTournamentTeam InTeam) {
        if (!IsNetworkAuthority())
            return;
        if (InTeam == ELeonTournamentTeam::Team1)
            ++Team1Kills;
        else if (InTeam == ELeonTournamentTeam::Team2)
            ++Team2Kills;
    }

    std::vector<ALeonTournamentPlayerState*> ALeonTournamentGameState::GetSortedScoreboard() const {
        std::vector<ALeonTournamentPlayerState*> rows;
        for (APlayerState* ps : GetPlayerArraySortedByScore()) {
            if (auto* sps = dynamic_cast<ALeonTournamentPlayerState*>(ps))
                rows.push_back(sps);
        }
        std::sort(rows.begin(), rows.end(), [](ALeonTournamentPlayerState* a, ALeonTournamentPlayerState* b) {
            if (a->GetScore() != b->GetScore())
                return a->GetScore() > b->GetScore();
            if (a->GetKills() != b->GetKills())
                return a->GetKills() > b->GetKills();
            if (a->GetDeaths() != b->GetDeaths())
                return a->GetDeaths() < b->GetDeaths();
            return a->GetPlayerName() < b->GetPlayerName();
        });
        return rows;
    }

    void ALeonTournamentGameState::SerializeReplication(std::vector<uint8_t>& OutBytes) const {
        AGameState::SerializeReplication(OutBytes);
        FNetBlob::WriteU8(OutBytes, static_cast<uint8_t>(MatchState));
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
        if (bytes.size() < 9)
            return;
        AGameState::DeserializeReplication(InData, InSize);
        // Skip the AGameState prefix: u8 match + f32 remaining + f32 elapsed.
        offset = 1 + 4 + 4;
        uint8_t state = 0, winner = 0;
        if (!FNetBlob::ReadU8(bytes, offset, state) || !FNetBlob::ReadF32(bytes, offset, CountdownRemaining) ||
            !FNetBlob::ReadI32(bytes, offset, Team1Kills) || !FNetBlob::ReadI32(bytes, offset, Team2Kills) ||
            !FNetBlob::ReadI32(bytes, offset, Team1PlayerCount) || !FNetBlob::ReadI32(bytes, offset, Team2PlayerCount) ||
            !FNetBlob::ReadU8(bytes, offset, winner))
            return;
        MatchState = static_cast<ELeonTournamentMatchState>(state);
        MatchWinner = static_cast<ELeonTournamentMatchWinner>(winner);
    }

} // namespace Leon
