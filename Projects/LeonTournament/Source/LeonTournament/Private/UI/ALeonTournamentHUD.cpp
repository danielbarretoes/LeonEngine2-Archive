#include "ALeonTournamentHUD.hpp"
#include "ALeonTournamentPlayerController.hpp"
#include "ALeonTournamentGameState.hpp"
#include "ALeonTournamentTransitionGameMode.hpp"
#include "ALeonTournamentAnimLabGameMode.hpp"
#include "ALeonTournamentRenderLabGameMode.hpp"
#include "ALeonTournamentBotController.hpp"
#include "ALeonTournamentCharacter.hpp"
#include "ALeonTournamentPlayerState.hpp"
#include "ALeonTournamentWeapon.hpp"
#include "Gameplay/UGameplayStatics.hpp"
#include "Gameplay/AAIController.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Gameplay/FGameplayDebugger.hpp"
#include "Gameplay/UCharacterMovementComponent.hpp"
#include "Engine/UWorld.hpp"
#include "Core/FApplication.hpp"
#include "Core/FInput.hpp"
#include "UMG/UUserWidget.hpp"
#include "ULeonTournamentGameInstance.hpp"
#include "Engine/UEngine.hpp"
#include "AI/UNavigationSystem.hpp"
#include "AI/FNavTypes.hpp"
#include "Renderer/FDebugRenderer.hpp"

#include <cstdio>

namespace Leon {

    namespace {
        const char* NetModeName(ENetMode InMode) {
            switch (InMode) {
            case ENetMode::ListenServer:
                return "ListenServer";
            case ENetMode::Client:
                return "Client";
            default:
                return "Standalone";
            }
        }

        const char* MatchStateName(ELeonTournamentMatchState InState) {
            switch (InState) {
            case ELeonTournamentMatchState::Lobby:
                return "Lobby";
            case ELeonTournamentMatchState::Starting:
                return "Starting";
            case ELeonTournamentMatchState::Playing:
                return "Playing";
            case ELeonTournamentMatchState::Finished:
                return "Finished";
            default:
                return "MainMenu";
            }
        }

        void HandleAIDebugInput(UWorld* InWorld) {
            if (!InWorld || !FGameplayDebugger::ShowAI())
                return;
            static bool bComma = false, bPeriod = false;
            const bool comma = FInput::IsKeyPressed(Key::Comma);
            const bool period = FInput::IsKeyPressed(Key::Period);
            if (comma && !bComma)
                FGameplayDebugger::CycleSelectedAI(1);
            if (period && !bPeriod)
                FGameplayDebugger::CycleSelectedAI(-1);
            bComma = comma;
            bPeriod = period;
        }

        void DrawAITelemetry(UWorld* InWorld) {
            if (!InWorld || !FGameplayDebugger::ShowAI())
                return;

            int32_t bots = 0, activeBt = 0, moving = 0, searching = 0, combat = 0, reloading = 0, dead = 0;
            int32_t pathsValid = 0, pathsFailed = 0;
            for (AAIController* ai : InWorld->GetAIControllers()) {
                auto* bot = dynamic_cast<ALeonTournamentBotController*>(ai);
                if (!bot)
                    continue;
                ++bots;
                if (bot->IsBehaviorTreeRunning())
                    ++activeBt;
                switch (bot->GetBotState()) {
                case ELeonTournamentBotState::Search:
                    ++searching;
                    break;
                case ELeonTournamentBotState::Combat:
                case ELeonTournamentBotState::Aim:
                case ELeonTournamentBotState::Fire:
                case ELeonTournamentBotState::MoveToTarget:
                    ++combat;
                    break;
                case ELeonTournamentBotState::Reload:
                    ++reloading;
                    break;
                case ELeonTournamentBotState::Dead:
                    ++dead;
                    break;
                default:
                    break;
                }
                if (bot->GetMoveStatus() == EPathFollowingStatus::Moving)
                    ++moving;
                if (bot->GetPathStatus() == ENavPathStatus::Valid)
                    ++pathsValid;
                else if (bot->GetPathStatus() == ENavPathStatus::Invalid)
                    ++pathsFailed;
            }
            if (auto* nav = InWorld->GetNavigationSystem()) {
                pathsValid = static_cast<int32_t>(nav->GetPathsValid());
                pathsFailed = static_cast<int32_t>(nav->GetPathsFailed());
            }

            char line[256];
            int key = 9300;
            auto put = [&](const char* text) { PrintString(text, 0.16f, glm::vec4(1.0f, 0.85f, 0.35f, 1.0f), key++); };
            std::snprintf(line, sizeof(line),
                          "AI  Bots:%d  Active BT:%d  Moving:%d  Searching:%d  Combat:%d  Reloading:%d  Dead:%d", bots,
                          activeBt, moving, searching, combat, reloading, dead);
            put(line);
            std::snprintf(line, sizeof(line), "Paths valid:%d  failed:%d  [Shift+F5 AI] [, next] [. prev]",
                          pathsValid, pathsFailed);
            put(line);

            const auto& ais = InWorld->GetAIControllers();
            if (ais.empty())
                return;
            int32_t n = static_cast<int32_t>(ais.size());
            int32_t idx = FGameplayDebugger::GetSelectedAIIndex() % n;
            if (idx < 0)
                idx += n;
            auto* selected = dynamic_cast<ALeonTournamentBotController*>(ais[static_cast<size_t>(idx)]);
            if (!selected)
                return;
            auto* pawn = selected->GetPawn<ALeonTournamentCharacter>();
            auto* ps = dynamic_cast<ALeonTournamentPlayerState*>(selected->GetPlayerState());
            auto* tgt = selected->GetCurrentTarget();
            auto brain = selected->GetBrainComponent();
            auto board = selected->GetBlackboardComponent();
            glm::vec3 dest = selected->GetMoveDestination();
            glm::vec3 vel =
                pawn && pawn->GetCharacterMovement() ? pawn->GetCharacterMovement()->GetVelocity() : glm::vec3(0.0f);
            std::snprintf(
                line, sizeof(line), "SEL[%d] team=%s state=%s node=%s tgt=%s dist=%.1f los=%s hp=%.0f ammo=%d",
                ps ? ps->GetPlayerId() : idx, ps ? LeonTournamentTeamName(ps->GetTeam()) : "-",
                LeonTournamentBotStateName(selected->GetBotState()), brain ? brain->GetActiveNodeName().c_str() : "-",
                tgt ? tgt->GetName().c_str() : "-", board ? board->GetValueAsFloat("DistanceToTarget") : 0.0f,
                board && board->GetValueAsBool("HasLineOfSight") ? "yes" : "no",
                pawn && pawn->GetHealthComponent() ? pawn->GetHealthComponent()->GetHealth() : 0.0f,
                pawn && pawn->GetWeapon() ? pawn->GetWeapon()->GetCurrentAmmo() : 0);
            put(line);
            std::snprintf(line, sizeof(line), "move=%s path=%s len=%.1f dest=(%.1f,%.1f,%.1f) v=%.2f",
                          PathFollowingStatusName(selected->GetMoveStatus()),
                          NavPathStatusName(selected->GetPathStatus()), selected->GetPathLength(), dest.x, dest.y,
                          dest.z, glm::length(glm::vec3(vel.x, 0.0f, vel.z)));
            put(line);
        }

        void DrawGameplayDebugOverlay(UWorld* InWorld, APlayerController* InPC) {
            HandleAIDebugInput(InWorld);
            DrawAITelemetry(InWorld);

            if (!InWorld || !FApplication::HasInstance() || !FApplication::Get().IsGameplayDebugEnabled())
                return;

            auto* gs = dynamic_cast<ALeonTournamentGameState*>(InWorld->GetGameState());
            auto* ch = InPC ? InPC->GetPawn<ALeonTournamentCharacter>() : nullptr;
            auto* ps = InPC ? dynamic_cast<ALeonTournamentPlayerState*>(InPC->GetPlayerState()) : nullptr;
            const auto& last = FDebugRenderer::GetLastTrace();

            char line[192];
            int key = 9100;
            auto put = [&](const char* text) { PrintString(text, 0.15f, glm::vec4(0.85f, 0.95f, 1.0f, 1.0f), key++); };

            std::snprintf(line, sizeof(line), "DEBUG  map=%s  net=%s  role=%s", InWorld->GetName().c_str(),
                          NetModeName(InWorld->GetNetMode()),
                          InWorld->GetFirstPlayerController() == InPC ? "Local" : "Remote");
            put(line);
            std::snprintf(line, sizeof(line), "PlayerId=%d  PS=%s  GM=%s  GS=%s", ps ? ps->GetPlayerId() : -1,
                          ps ? ps->GetPlayerName().c_str() : "-", InWorld->GetGameMode() ? "yes" : "no",
                          gs ? MatchStateName(gs->GetMatchState()) : "-");
            put(line);
            std::snprintf(line, sizeof(line), "capsule=%.2fm  eye=%.2fm  hp=%.0f  ammo=%d/%d",
                          ch ? ch->GetCapsuleHeight() : 0.0f, ch ? ch->GetEyeHeight() : 0.0f,
                          ch && ch->GetHealthComponent() ? ch->GetHealthComponent()->GetHealth() : 0.0f,
                          ch && ch->GetWeapon() ? ch->GetWeapon()->GetCurrentAmmo() : 0,
                          ch && ch->GetWeapon() ? ch->GetWeapon()->GetMagazineSize() : 0);
            put(line);
            if (last.bValid) {
                std::snprintf(line, sizeof(line), "trace ch=%u  %s  hit=(%.1f,%.1f,%.1f)", last.Channel,
                              last.bHit ? "HIT" : "MISS", last.Hit.x, last.Hit.y, last.Hit.z);
                put(line);
            }
            for (AAIController* ai : InWorld->GetAIControllers()) {
                auto* bot = dynamic_cast<ALeonTournamentBotController*>(ai);
                if (!bot)
                    continue;
                auto* bps = bot->GetPlayerState();
                auto* tgt = bot->GetCurrentTarget();
                std::snprintf(line, sizeof(line), "BOT[%d] %s  tgt=%s", bps ? bps->GetPlayerId() : -1,
                              LeonTournamentBotStateName(bot->GetBotState()), tgt ? tgt->GetName().c_str() : "-");
                put(line);
            }
        }
    } // namespace

    ALeonTournamentHUD::ALeonTournamentHUD(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AHUD(InHandle, InWorld, InName) {
        SetClass("ALeonTournamentHUD");
    }

    void ALeonTournamentHUD::BeginPlay() {
        AHUD::BeginPlay();
        if (!PlayerController)
            return;
        MenuWidget = UUserWidget::CreateWidget<ULeonTournamentMainMenuWidget>(PlayerController);
        LobbyWidget = UUserWidget::CreateWidget<ULeonTournamentLobbyWidget>(PlayerController);
        HudWidget = UUserWidget::CreateWidget<ULeonTournamentHUDWidget>(PlayerController);
        ScoreboardWidget = UUserWidget::CreateWidget<ULeonTournamentScoreboardWidget>(PlayerController);
        PauseWidget = UUserWidget::CreateWidget<ULeonTournamentPauseWidget>(PlayerController);
        EndWidget = UUserWidget::CreateWidget<ULeonTournamentMatchEndWidget>(PlayerController);
        RenderLabWidget = UUserWidget::CreateWidget<ULeonTournamentRenderLabWidget>(PlayerController);
        LoadingOverlay = UUserWidget::CreateWidget<ULeonTournamentLoadingOverlayWidget>(PlayerController);

        MenuWidget->AddToViewport(10);
        LobbyWidget->AddToViewport(10);
        HudWidget->AddToViewport(1);
        ScoreboardWidget->AddToViewport(15);
        PauseWidget->AddToViewport(40);
        EndWidget->AddToViewport(20);
        RenderLabWidget->AddToViewport(30);
        if (LoadingOverlay) {
            LoadingOverlay->AddToViewport(100);
            LoadingOverlay->SetVisibility(ESlateVisibility::Collapsed);
        }

        LobbyWidget->SetVisibility(ESlateVisibility::Collapsed);
        HudWidget->SetVisibility(ESlateVisibility::Collapsed);
        ScoreboardWidget->SetVisibility(ESlateVisibility::Collapsed);
        PauseWidget->SetVisibility(ESlateVisibility::Collapsed);
        EndWidget->SetVisibility(ESlateVisibility::Collapsed);
        RenderLabWidget->SetVisibility(ESlateVisibility::Collapsed);
        ShownState = ELeonTournamentMatchState::MainMenu;
        PrevState = ShownState;
    }

    void ALeonTournamentHUD::SyncWidgets() {
        auto* gs = World ? dynamic_cast<ALeonTournamentGameState*>(World->GetGameState()) : nullptr;
        ELeonTournamentMatchState state = gs ? gs->GetMatchState() : ELeonTournamentMatchState::MainMenu;
        if (state == ELeonTournamentMatchState::Finished && PrevState != ELeonTournamentMatchState::Finished) {
            UGameplayStatics::PlaySound2D("/Game/Audio/SFX_MatchEnd", 0.9f);
            if (auto* spc = dynamic_cast<ALeonTournamentPlayerController*>(PlayerController))
                spc->PushBanner("MATCH OVER", 2.5f, {0.95f, 0.9f, 1.0f, 1.0f});
        }
        PrevState = state;
        ShownState = state;

        auto show = [](const TRef<UUserWidget>& w, bool bOn, bool bHit = true) {
            if (!w)
                return;
            if (!bOn)
                w->SetVisibility(ESlateVisibility::Collapsed);
            else
                w->SetVisibility(bHit ? ESlateVisibility::Visible : ESlateVisibility::HitTestInvisible);
        };
        const bool bRenderLab =
            World && dynamic_cast<ALeonTournamentRenderLabGameMode*>(World->GetGameMode()) != nullptr;
        show(MenuWidget, state == ELeonTournamentMatchState::MainMenu && !bRenderLab);
        show(LobbyWidget, state == ELeonTournamentMatchState::Lobby);
        show(HudWidget,
             !bRenderLab &&
                 (state == ELeonTournamentMatchState::Playing || state == ELeonTournamentMatchState::Starting),
             false);
        show(EndWidget, state == ELeonTournamentMatchState::Finished);
        show(RenderLabWidget, bRenderLab && state == ELeonTournamentMatchState::Playing, true);

        if (LoadingOverlay && LoadingOverlay->IsVisible()) {
            if (state == ELeonTournamentMatchState::Lobby || state == ELeonTournamentMatchState::Starting ||
                state == ELeonTournamentMatchState::Playing)
                HideLoadingOverlay();
            else if (state == ELeonTournamentMatchState::MainMenu && World &&
                     dynamic_cast<ALeonTournamentTransitionGameMode*>(World->GetGameMode()) == nullptr) {
                if (UEngine::HasInstance()) {
                    if (auto* gi = dynamic_cast<ULeonTournamentGameInstance*>(UEngine::Get().GetGameInstance().get())) {
                        if (!gi->IsLoadingOverlayActive())
                            HideLoadingOverlay();
                    }
                }
            }
        }
    }

    void ALeonTournamentHUD::ShowLoadingOverlay(const std::string& InLabel) {
        if (!LoadingOverlay)
            return;
        LoadingOverlay->SetStatusText(InLabel);
        LoadingOverlay->SetVisibility(ESlateVisibility::HitTestInvisible);
        if (UEngine::HasInstance()) {
            if (auto* gi = dynamic_cast<ULeonTournamentGameInstance*>(UEngine::Get().GetGameInstance().get()))
                gi->SetLoadingOverlayActive(true, InLabel);
        }
    }

    void ALeonTournamentHUD::HideLoadingOverlay() {
        if (LoadingOverlay)
            LoadingOverlay->SetVisibility(ESlateVisibility::Collapsed);
        if (UEngine::HasInstance()) {
            if (auto* gi = dynamic_cast<ULeonTournamentGameInstance*>(UEngine::Get().GetGameInstance().get()))
                gi->SetLoadingOverlayActive(false);
        }
    }

    void ALeonTournamentHUD::Tick(float DeltaSeconds) {
        AHUD::Tick(DeltaSeconds);
        SyncWidgets();

        auto* spc = dynamic_cast<ALeonTournamentPlayerController*>(PlayerController);
        const bool bInMatch =
            ShownState == ELeonTournamentMatchState::Playing || ShownState == ELeonTournamentMatchState::Starting;
        const bool bAnimLab = World && dynamic_cast<ALeonTournamentAnimLabGameMode*>(World->GetGameMode()) != nullptr;
        const bool bRenderLab =
            World && dynamic_cast<ALeonTournamentRenderLabGameMode*>(World->GetGameMode()) != nullptr;
        if (spc && bInMatch && !bAnimLab && !bRenderLab && spc->ConsumeEscapePressed())
            spc->TogglePauseMenu();

        if (PauseWidget) {
            const bool bPause = spc && !bRenderLab && spc->IsPauseMenuOpen() && bInMatch;
            PauseWidget->SetVisibility(bPause ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        }

        const bool bWantSb = spc && !spc->IsPauseMenuOpen() && spc->IsScoreboardHeld() && bInMatch && !bRenderLab;
        if (ScoreboardWidget) {
            ScoreboardWidget->SetVisibility(bWantSb ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
            bScoreboardVisible = bWantSb;
        }

        DrawGameplayDebugOverlay(World, PlayerController);
    }

} // namespace Leon
