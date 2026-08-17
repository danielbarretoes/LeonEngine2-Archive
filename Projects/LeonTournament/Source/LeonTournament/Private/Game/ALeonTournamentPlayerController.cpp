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
        constexpr float kKillConfirmSeconds = 1.15f;
        constexpr float kDamageFlashSeconds = 0.22f;
        constexpr float kKillStreakWindow = 3.25f;
    } // namespace

    ALeonTournamentPlayerController::ALeonTournamentPlayerController(entt::entity InHandle, UWorld* InWorld,
                                                                     const std::string& InName)
        : APlayerController(InHandle, InWorld, InName) {
        SetClass("ALeonTournamentPlayerController");
    }

    void ALeonTournamentPlayerController::Tick(float DeltaSeconds) {
        APlayerController::Tick(DeltaSeconds);
        auto* gs = World ? dynamic_cast<ALeonTournamentGameState*>(World->GetGameState()) : nullptr;
        const ELeonTournamentMatchState state =
            gs ? gs->GetMatchState() : ELeonTournamentMatchState::MainMenu;
        const bool bInMatch =
            state == ELeonTournamentMatchState::Playing || state == ELeonTournamentMatchState::Starting;

        if (bPauseMenuOpen && bInMatch) {
            SetInputModeUIOnly();
            SetShowMouseCursor(true);
        } else if (bInMatch) {
            SetInputModeGameOnly();
            SetShowMouseCursor(false);
        } else {
            SetInputModeUIOnly();
            SetShowMouseCursor(true);
            bPauseMenuOpen = false;
        }

        bScoreboardHeld = !bPauseMenuOpen && FInput::IsKeyPressed(Key::Tab);
        const FInputSettings& input = FInputSettings::Get();
        if (!bPauseMenuOpen && input.bEnableGamepad && FInput::IsGamepadConnected(input.GamepadId) &&
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
        BannerRemaining = std::max(0.0f, BannerRemaining - DeltaSeconds);
        KillStreakWindowRemaining = std::max(0.0f, KillStreakWindowRemaining - DeltaSeconds);
        if (KillStreakWindowRemaining <= 0.0f)
            KillStreak = 0;
        if (KillConfirmRemaining <= 0.0f)
            KillFeedText.clear();
        if (BannerRemaining <= 0.0f)
            BannerText.clear();
    }

    bool ALeonTournamentPlayerController::ConsumeEscapePressed() {
        const bool pressed = bEscapePressed;
        bEscapePressed = false;
        return pressed;
    }

    void ALeonTournamentPlayerController::SetPauseMenuOpen(bool bOpen) {
        if (bPauseMenuOpen == bOpen)
            return;
        bPauseMenuOpen = bOpen;
        UGameplayStatics::PlaySound2D(bOpen ? "/Game/Audio/SFX_PauseOpen" : "/Game/Audio/SFX_UIClick", 0.55f);
    }

    void ALeonTournamentPlayerController::TogglePauseMenu() {
        SetPauseMenuOpen(!bPauseMenuOpen);
    }

    void ALeonTournamentPlayerController::PushBanner(const std::string& InText, float InSeconds,
                                                     const glm::vec4& InColor) {
        BannerText = InText;
        BannerColor = InColor;
        BannerRemaining = InSeconds;
    }

    void ALeonTournamentPlayerController::ClearBanner() {
        BannerText.clear();
        BannerRemaining = 0.0f;
    }

    void ALeonTournamentPlayerController::NotifyConfirmedHit(bool bKill) {
        HitMarkerRemaining = kHitMarkerSeconds;
        if (!bKill) {
            UGameplayStatics::PlaySound2D("/Game/Audio/SFX_HitConfirm", 0.55f);
            return;
        }

        if (KillStreakWindowRemaining > 0.0f)
            ++KillStreak;
        else
            KillStreak = 1;
        KillStreakWindowRemaining = kKillStreakWindow;
        KillConfirmRemaining = kKillConfirmSeconds;

        if (KillStreak >= 3) {
            KillFeedText = "TRIPLE KILL";
            PushBanner("TRIPLE KILL", 1.6f, {1.0f, 0.55f, 0.15f, 1.0f});
            UGameplayStatics::PlaySound2D("/Game/Audio/SFX_TripleKill", 0.9f);
        } else if (KillStreak == 2) {
            KillFeedText = "DOUBLE KILL";
            PushBanner("DOUBLE KILL", 1.4f, {1.0f, 0.75f, 0.2f, 1.0f});
            UGameplayStatics::PlaySound2D("/Game/Audio/SFX_DoubleKill", 0.85f);
        } else {
            KillFeedText = "KILL";
            PushBanner("KILL", 1.0f, {1.0f, 0.85f, 0.25f, 1.0f});
            UGameplayStatics::PlaySound2D("/Game/Audio/SFX_KillConfirm", 0.8f);
        }
    }

    void ALeonTournamentPlayerController::NotifyTookDamage() {
        DamageFlashRemaining = kDamageFlashSeconds;
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_DamageTaken", 0.7f);
    }

    void ALeonTournamentPlayerController::NotifyLocalDeath() {
        KillStreak = 0;
        KillStreakWindowRemaining = 0.0f;
        KillFeedText.clear();
        PushBanner("YOU DIED", 2.2f, {1.0f, 0.35f, 0.3f, 1.0f});
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_YouDied", 0.85f);
    }

} // namespace Leon
