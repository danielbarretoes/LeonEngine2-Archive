#include "ALeonTournamentPlayerController.hpp"
#include "ALeonTournamentGameState.hpp"
#include "Core/FInput.hpp"
#include "Core/FInputSettings.hpp"
#include "Engine/UWorld.hpp"
#include "Gameplay/UGameplayStatics.hpp"

#include <algorithm>

namespace Leon {

    namespace {
        constexpr float kHitMarkerSeconds = 0.18f;
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
            if (state == ELeonTournamentMatchState::Playing || state == ELeonTournamentMatchState::Starting) {
                SetInputModeGameOnly();
                SetShowMouseCursor(false);
            } else {
                SetInputModeUIOnly();
                SetShowMouseCursor(true);
            }
        }
        bScoreboardHeld = FInput::IsKeyPressed(Key::Tab);
        const FInputSettings& input = FInputSettings::Get();
        if (input.bEnableGamepad && FInput::IsGamepadConnected(input.GamepadId) &&
            (FInput::IsGamepadButtonPressed(GamepadButton::Back, input.GamepadId) ||
             FInput::IsGamepadButtonPressed(GamepadButton::Y, input.GamepadId)))
            bScoreboardHeld = true;

        const bool bEsc =
            FInput::IsKeyPressed(Key::Escape) ||
            (input.bEnableGamepad && FInput::IsGamepadConnected(input.GamepadId) &&
             FInput::IsGamepadButtonPressed(GamepadButton::Start, input.GamepadId));
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
        if (bKill) {
            KillConfirmRemaining = kKillConfirmSeconds;
            UGameplayStatics::PlaySound2D("/Game/Audio/SFX_KillConfirm", 0.75f);
        }
    }

    void ALeonTournamentPlayerController::NotifyTookDamage() {
        DamageFlashRemaining = kDamageFlashSeconds;
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_DamageTaken", 0.7f);
    }

} // namespace Leon
