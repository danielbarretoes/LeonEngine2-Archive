#include "Gameplay/AGameMode.hpp"

namespace Leon {

    AGameMode::AGameMode(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AGameModeBase(InHandle, InWorld, InName) {
        SetClass("AGameMode");
        GameStateClass = "AGameState";
    }

    AGameState* AGameMode::GetMatchGameState() const {
        return dynamic_cast<AGameState*>(GameState);
    }

    void AGameMode::StartMatch() {
        if (!IsNetworkAuthority())
            return;
        if (HasMatchStarted() && !HasMatchEnded())
            return;
        if (AGameState* gs = GetMatchGameState())
            gs->SetMatchState(EMatchState::InProgress);
    }

    void AGameMode::EndMatch() {
        if (!IsNetworkAuthority())
            return;
        if (AGameState* gs = GetMatchGameState()) {
            gs->SetMatchState(EMatchState::WaitingPostMatch);
            gs->SetRemainingTime(0.0f);
        }
    }

    bool AGameMode::HasMatchStarted() const {
        AGameState* gs = GetMatchGameState();
        return gs && gs->GetMatchState() != EMatchState::WaitingToStart;
    }

    bool AGameMode::HasMatchEnded() const {
        AGameState* gs = GetMatchGameState();
        return gs && gs->GetMatchState() == EMatchState::WaitingPostMatch;
    }

} // namespace Leon
