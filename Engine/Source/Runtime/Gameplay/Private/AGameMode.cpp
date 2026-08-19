#include "Gameplay/AGameMode.hpp"

namespace Leon {

    AGameMode::AGameMode(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AGameModeBase(InHandle, InWorld, InName) {
        SetClass("AGameMode");
        GameStateClass = "AGameState";
    }

    AGameState* AGameMode::GetGameState() const {
        return dynamic_cast<AGameState*>(GameState);
    }

    void AGameMode::SetMatchState(EMatchState InState) {
        if (!IsNetworkAuthority())
            return;
        AGameState* gs = GetGameState();
        if (!gs)
            return;
        const EMatchState previous = gs->GetMatchState();
        if (previous == InState)
            return;
        gs->SetMatchState(InState);
        if (InState == EMatchState::WaitingPostMatch)
            gs->SetRemainingTime(0.0f);
        // Flow: match state
        // 1. GameMode is the authority writer.
        // 2. GameState replicates MatchState.
        // 3. Handles run after the GameState field updates.
        switch (InState) {
        case EMatchState::WaitingToStart:
            HandleMatchIsWaitingToStart();
            break;
        case EMatchState::InProgress:
            HandleMatchHasStarted();
            break;
        case EMatchState::WaitingPostMatch:
            HandleMatchHasEnded();
            break;
        }
    }

    void AGameMode::StartMatch() {
        if (!IsNetworkAuthority())
            return;
        if (HasMatchInProgress())
            return;
        SetMatchState(EMatchState::InProgress);
    }

    void AGameMode::EndMatch() {
        if (!IsNetworkAuthority())
            return;
        if (HasMatchEnded())
            return;
        SetMatchState(EMatchState::WaitingPostMatch);
    }

    void AGameMode::RestartGame() {
        if (!IsNetworkAuthority())
            return;
        SetMatchState(EMatchState::WaitingToStart);
        StartMatch();
    }

    void AGameMode::HandleMatchIsWaitingToStart() {}

    void AGameMode::HandleMatchHasStarted() {}

    void AGameMode::HandleMatchHasEnded() {}

    bool AGameMode::PlayerCanRestart(AController* InPlayer) const {
        (void)InPlayer;
        return HasMatchInProgress();
    }

    bool AGameMode::HasMatchStarted() const {
        AGameState* gs = GetGameState();
        return gs && gs->GetMatchState() != EMatchState::WaitingToStart;
    }

    bool AGameMode::HasMatchEnded() const {
        AGameState* gs = GetGameState();
        return gs && gs->GetMatchState() == EMatchState::WaitingPostMatch;
    }

    bool AGameMode::HasMatchInProgress() const {
        AGameState* gs = GetGameState();
        return gs && gs->GetMatchState() == EMatchState::InProgress;
    }

} // namespace Leon
