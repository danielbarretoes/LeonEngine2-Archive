#include "ALeonTournamentPlayerController.hpp"
#include "ALeonTournamentGameState.hpp"
#include "Core/FInput.hpp"
#include "Core/FInputSettings.hpp"
#include "ALeonTournamentGameMode.hpp"
#include "ALeonTournamentCharacter.hpp"
#include "ALeonTournamentPlayerState.hpp"
#include "Engine/UWorld.hpp"
#include "Gameplay/UGameplayStatics.hpp"
#include "Gameplay/APawn.hpp"
#include "Gameplay/AController.hpp"

#include <algorithm>

namespace Leon {

    namespace {
        constexpr float kHitMarkerSeconds = 0.18f;
        constexpr float kKillConfirmSeconds = 1.15f;
        constexpr float kDamageFlashSeconds = 0.22f;
        constexpr float kKillStreakWindow = 3.25f;

        std::string SpectatorDisplayNameForPawn(const APawn* InPawn) {
            if (!InPawn)
                return "Unknown";
            if (const auto* ch = dynamic_cast<const ALeonTournamentCharacter*>(InPawn)) {
                if (const AController* ctrl = ch->GetController()) {
                    if (const auto* ps = dynamic_cast<const ALeonTournamentPlayerState*>(ctrl->GetPlayerState()))
                        return ps->GetPlayerName();
                }
            }
            return InPawn->GetName();
        }
    } // namespace

    ALeonTournamentPlayerController::ALeonTournamentPlayerController(entt::entity InHandle, UWorld* InWorld,
                                                                     const std::string& InName)
        : APlayerController(InHandle, InWorld, InName) {
        SetClass("ALeonTournamentPlayerController");
    }

    void ALeonTournamentPlayerController::Tick(float DeltaSeconds) {
        APlayerController::Tick(DeltaSeconds);
        auto* gs = World ? dynamic_cast<ALeonTournamentGameState*>(World->GetGameState()) : nullptr;
        const ELeonTournamentMatchState state = gs ? gs->GetMatchState() : ELeonTournamentMatchState::MainMenu;
        const bool bInMatch =
            state == ELeonTournamentMatchState::Playing || state == ELeonTournamentMatchState::Starting;
        auto* gm = World ? dynamic_cast<ALeonTournamentGameMode*>(World->GetGameMode()) : nullptr;
        const bool bUICursor = gm && gm->WantsUICursor();

        if (bUICursor) {
            SetInputModeUIOnly();
            SetShowMouseCursor(true);
        } else if (bPauseMenuOpen && bInMatch) {
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

        const bool bEsc = FInput::IsKeyPressed(Key::Escape) ||
                          (input.bEnableGamepad && FInput::IsGamepadConnected(input.GamepadId) &&
                           FInput::IsGamepadButtonPressed(GamepadButton::Start, input.GamepadId));
        bEscapePressed = bEsc && !bEscapeWasDown;
        bEscapeWasDown = bEsc;

        HitMarkerRemaining = std::max(0.0f, HitMarkerRemaining - DeltaSeconds);
        KillConfirmRemaining = std::max(0.0f, KillConfirmRemaining - DeltaSeconds);
        DamageFlashRemaining = std::max(0.0f, DamageFlashRemaining - DeltaSeconds);
        BannerRemaining = std::max(0.0f, BannerRemaining - DeltaSeconds);
        DamageIndicatorRemaining = std::max(0.0f, DamageIndicatorRemaining - DeltaSeconds);
        KillStreakWindowRemaining = std::max(0.0f, KillStreakWindowRemaining - DeltaSeconds);
        if (KillStreakWindowRemaining <= 0.0f)
            KillStreak = 0;
        if (KillConfirmRemaining <= 0.0f)
            KillFeedText.clear();
        if (BannerRemaining <= 0.0f)
            BannerText.clear();

        if (IsSpectating() && bInMatch && !bPauseMenuOpen && IsGameInputAllowed()) {
            const bool bPrev = FInput::IsKeyPressed(Key::Q);
            const bool bNext = FInput::IsKeyPressed(Key::E);
            if (bPrev && !bSpectatorPrevWasDown)
                CycleSpectatorTarget(-1);
            if (bNext && !bSpectatorNextWasDown)
                CycleSpectatorTarget(1);
            bSpectatorPrevWasDown = bPrev;
            bSpectatorNextWasDown = bNext;

            if (auto* target = dynamic_cast<APawn*>(GetViewTarget()))
                SpectatorTargetName = SpectatorDisplayNameForPawn(target);
            else
                SpectatorTargetName.clear();
        } else {
            bSpectatorPrevWasDown = false;
            bSpectatorNextWasDown = false;
            if (!IsSpectating())
                SpectatorTargetName.clear();
        }
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

    void ALeonTournamentPlayerController::NotifyConfirmedHit(bool bKill, bool bHeadshot) {
        HitMarkerRemaining = kHitMarkerSeconds;
        bHeadshotMarker = bHeadshot;
        if (!bKill) {
            UGameplayStatics::PlaySound2D("/Game/Audio/SFX_HitConfirm", bHeadshot ? 0.75f : 0.55f);
            return;
        }

        bool bScoreKill = true;
        if (UWorld* world = GetWorld()) {
            if (auto* gs = dynamic_cast<ALeonTournamentGameState*>(world->GetGameState()))
                bScoreKill = gs->GetMatchState() == ELeonTournamentMatchState::Playing;
        }
        if (!bScoreKill) {
            UGameplayStatics::PlaySound2D("/Game/Audio/SFX_HitConfirm", bHeadshot ? 0.75f : 0.55f);
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
            UGameplayStatics::PlaySound2D("/Game/Audio/SFX_Announce", 0.55f);
        } else if (KillStreak == 2) {
            KillFeedText = "DOUBLE KILL";
            PushBanner("DOUBLE KILL", 1.4f, {1.0f, 0.75f, 0.2f, 1.0f});
            UGameplayStatics::PlaySound2D("/Game/Audio/SFX_DoubleKill", 0.85f);
            UGameplayStatics::PlaySound2D("/Game/Audio/SFX_Announce", 0.5f);
        } else {
            KillFeedText = "KILL";
            PushBanner("KILL", 1.0f, {1.0f, 0.85f, 0.25f, 1.0f});
            UGameplayStatics::PlaySound2D("/Game/Audio/SFX_KillConfirm", 0.8f);
            if (bHeadshot)
                UGameplayStatics::PlaySound2D("/Game/Audio/SFX_Announce", 0.45f);
        }
    }

    void ALeonTournamentPlayerController::NotifyTookDamage(float InYawDeg) {
        DamageFlashRemaining = kDamageFlashSeconds;
        DamageIndicatorYawDeg = InYawDeg;
        DamageIndicatorRemaining = 0.35f;
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_DamageTaken", 0.7f);
    }

    void ALeonTournamentPlayerController::NotifyLocalDeath() {
        KillStreak = 0;
        KillStreakWindowRemaining = 0.0f;
        KillFeedText.clear();
        PushBanner("YOU DIED", 2.2f, {1.0f, 0.35f, 0.3f, 1.0f});
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_YouDied", 0.85f);

        std::vector<APawn*> targets;
        CollectSpectatorTargets(targets);
        AActor* initial = targets.empty() ? nullptr : targets.front();
        EnterSpectatorMode(initial);
        SpectatorTargetName = initial ? SpectatorDisplayNameForPawn(dynamic_cast<APawn*>(initial)) : "";
    }

    void ALeonTournamentPlayerController::CollectSpectatorTargets(std::vector<APawn*>& OutTargets) const {
        OutTargets.clear();
        if (!World)
            return;

        const auto* localChar = GetPawn<ALeonTournamentCharacter>();
        const auto* localPs = dynamic_cast<const ALeonTournamentPlayerState*>(GetPlayerState());
        const ELeonTournamentTeam localTeam = localPs ? localPs->GetTeam() : ELeonTournamentTeam::None;

        ELeonTournamentGameModeId mode = ELeonTournamentGameModeId::TeamDeathmatch;
        if (const auto* gm = dynamic_cast<const ALeonTournamentGameMode*>(World->GetGameMode()))
            mode = gm->GetActiveGameMode();
        const bool bTeamMode = mode != ELeonTournamentGameModeId::FreeForAll;

        for (const auto& actor : World->GetAllActors()) {
            auto* ch = dynamic_cast<ALeonTournamentCharacter*>(actor.get());
            if (!ch || ch == localChar || ch->IsPendingKill() || ch->IsDeadFrozen())
                continue;
            if (bTeamMode && localTeam != ELeonTournamentTeam::None && ch->GetTeam() != localTeam)
                continue;
            OutTargets.push_back(ch);
        }

        std::sort(OutTargets.begin(), OutTargets.end(), [](const APawn* a, const APawn* b) {
            return SpectatorDisplayNameForPawn(a) < SpectatorDisplayNameForPawn(b);
        });
    }

} // namespace Leon
