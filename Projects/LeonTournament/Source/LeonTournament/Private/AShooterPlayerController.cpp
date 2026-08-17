#include "AShooterPlayerController.hpp"
#include "AShooterGameState.hpp"
#include "Core/FInput.hpp"
#include "Engine/UWorld.hpp"

#include <algorithm>

namespace Leon {

    namespace {
        constexpr float kHitMarkerSeconds = 0.12f;
        constexpr float kKillConfirmSeconds = 0.85f;
        constexpr float kDamageFlashSeconds = 0.22f;
    } // namespace

    AShooterPlayerController::AShooterPlayerController(entt::entity InHandle, UWorld* InWorld,
                                                       const std::string& InName)
        : APlayerController(InHandle, InWorld, InName) {
        SetClass("AShooterPlayerController");
    }

    void AShooterPlayerController::Tick(float DeltaSeconds) {
        APlayerController::Tick(DeltaSeconds);
        if (auto* gs = World ? dynamic_cast<AShooterGameState*>(World->GetGameState()) : nullptr) {
            const EShooterMatchState state = gs->GetMatchState();
            if (state == EShooterMatchState::Playing || state == EShooterMatchState::Starting)
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

    bool AShooterPlayerController::ConsumeEscapePressed() {
        const bool pressed = bEscapePressed;
        bEscapePressed = false;
        return pressed;
    }

    void AShooterPlayerController::NotifyConfirmedHit(bool bKill) {
        HitMarkerRemaining = kHitMarkerSeconds;
        if (bKill)
            KillConfirmRemaining = kKillConfirmSeconds;
    }

    void AShooterPlayerController::NotifyTookDamage() {
        DamageFlashRemaining = kDamageFlashSeconds;
    }

} // namespace Leon
