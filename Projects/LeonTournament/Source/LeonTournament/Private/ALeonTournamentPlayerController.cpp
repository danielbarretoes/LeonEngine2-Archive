#include "ALeonTournamentPlayerController.hpp"
#include "ALeonTournamentGameState.hpp"
#include "Core/FInput.hpp"
#include "Engine/UWorld.hpp"

#include <algorithm>

namespace Leon {

    namespace {
        constexpr float kHitMarkerSeconds = 0.12f;
        constexpr float kKillConfirmSeconds = 0.85f;
        constexpr float kDamageFlashSeconds = 0.22f;
    } // namespace

    ALeonTournamentPlayerController::ALeonTournamentPlayerController(entt::entity InHandle, UWorld* InWorld,
                                                       const std::string& InName)
        : APlayerController(InHandle, InWorld, InName) {
        SetClass("ALeonTournamentPlayerController");
    }

    void ALeonTournamentPlayerController::Tick(float DeltaSeconds) {
        APlayerController::Tick(DeltaSeconds);
        if (auto* gs = World ? dynamic_cast<ALeonTournamentGameState*>(World->GetGameState()) : nullptr) {
            const ELeonTournamentMatchState state = gs->GetMatchState();
            if (state == ELeonTournamentMatchState::Playing || state == ELeonTournamentMatchState::Starting)
                SetInputModeGameOnly();
            else
                SetInputModeUIOnly();
        }
        bScoreboardHeld = FInput::IsKeyPressed(Key::Tab);
        const bool bEsc = FInput::IsKeyPressed(Key::Escape);
        bEscapePressed = bEsc && !bEscapeWasDown;
        bEscapeWasDown = bEsc;

        HitMarkerRemaining = std::max(0.0f, HitMarkerRemaining - DeltaSeconds);
        KillConfirmRemaining = std::max(0.0f, KillConfirmRemaining - DeltaSeconds);
        DamageFlashRemaining = std::max(0.0f, DamageFlashRemaining - DeltaSeconds);
    }

    bool ALeonTournamentPlayerController::ConsumeEscapePressed() {
        const bool pressed = bEscapePressed;
        bEscapePressed = false;
        return pressed;
    }

    void ALeonTournamentPlayerController::NotifyConfirmedHit(bool bKill) {
        HitMarkerRemaining = kHitMarkerSeconds;
        if (bKill)
            KillConfirmRemaining = kKillConfirmSeconds;
    }

    void ALeonTournamentPlayerController::NotifyTookDamage() {
        DamageFlashRemaining = kDamageFlashSeconds;
    }

} // namespace Leon
