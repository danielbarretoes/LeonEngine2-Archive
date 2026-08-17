#include "AShooterGameState.hpp"
#include "AShooterPlayerState.hpp"

#include <algorithm>

namespace Leon {

    AShooterGameState::AShooterGameState(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AGameStateBase(InHandle, InWorld, InName) {
        SetClass("AShooterGameState");
    }

    void AShooterGameState::AddTeamKill(EShooterTeam InTeam) {
        if (InTeam == EShooterTeam::Team1)
            ++Team1Kills;
        else if (InTeam == EShooterTeam::Team2)
            ++Team2Kills;
    }

    std::vector<AShooterPlayerState*> AShooterGameState::GetSortedScoreboard() const {
        std::vector<AShooterPlayerState*> rows;
        for (APlayerState* ps : GetPlayerArray()) {
            if (auto* sps = dynamic_cast<AShooterPlayerState*>(ps))
                rows.push_back(sps);
        }
        std::sort(rows.begin(), rows.end(), [](AShooterPlayerState* a, AShooterPlayerState* b) {
            if (a->GetKills() != b->GetKills())
                return a->GetKills() > b->GetKills();
            return a->GetDeaths() < b->GetDeaths();
        });
        return rows;
    }

    void AShooterGameState::SerializeReplication(std::vector<uint8_t>& OutBytes) const {
        FNetBlob::WriteU8(OutBytes, static_cast<uint8_t>(MatchState));
        FNetBlob::WriteF32(OutBytes, RemainingTime);
        FNetBlob::WriteF32(OutBytes, CountdownRemaining);
        FNetBlob::WriteI32(OutBytes, Team1Kills);
        FNetBlob::WriteI32(OutBytes, Team2Kills);
        FNetBlob::WriteI32(OutBytes, Team1PlayerCount);
        FNetBlob::WriteI32(OutBytes, Team2PlayerCount);
        FNetBlob::WriteU8(OutBytes, static_cast<uint8_t>(MatchWinner));
    }

    void AShooterGameState::DeserializeReplication(const uint8_t* InData, size_t InSize) {
        std::vector<uint8_t> bytes(InData, InData + InSize);
        size_t offset = 0;
        uint8_t state = 0, winner = 0;
        if (!FNetBlob::ReadU8(bytes, offset, state) || !FNetBlob::ReadF32(bytes, offset, RemainingTime) ||
            !FNetBlob::ReadF32(bytes, offset, CountdownRemaining) || !FNetBlob::ReadI32(bytes, offset, Team1Kills) ||
            !FNetBlob::ReadI32(bytes, offset, Team2Kills) || !FNetBlob::ReadI32(bytes, offset, Team1PlayerCount) ||
            !FNetBlob::ReadI32(bytes, offset, Team2PlayerCount) || !FNetBlob::ReadU8(bytes, offset, winner))
            return;
        MatchState = static_cast<EShooterMatchState>(state);
        MatchWinner = static_cast<EShooterMatchWinner>(winner);
    }

} // namespace Leon
