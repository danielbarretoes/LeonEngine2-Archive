#include "AShooterHUD.hpp"
#include "AShooterGameState.hpp"
#include "AShooterPlayerController.hpp"
#include "AShooterPlayerState.hpp"
#include "AShooterCharacter.hpp"
#include "AShooterWeapon.hpp"
#include "AShooterBotController.hpp"
#include "Core/FApplication.hpp"
#include "Core/FInput.hpp"
#include "Engine/UWorld.hpp"
#include "Gameplay/AAIController.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Gameplay/UGameplayStatics.hpp"
#include "Gameplay/FGameplayDebugger.hpp"
#include "Gameplay/UCharacterMovementComponent.hpp"
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

        const char* MatchStateName(EShooterMatchState InState) {
            switch (InState) {
            case EShooterMatchState::Lobby:
                return "Lobby";
            case EShooterMatchState::Starting:
                return "Starting";
            case EShooterMatchState::Playing:
                return "Playing";
            case EShooterMatchState::Finished:
                return "Finished";
            default:
                return "MainMenu";
            }
        }

        void HandleAIDebugInput(UWorld* InWorld) {
            if (!InWorld || !FGameplayDebugger::ShowAI())
                return;
            static bool bComma = false, bPeriod = false, bF7 = false, bF8 = false;
            const bool comma = FInput::IsKeyPressed(Key::Comma);
            const bool period = FInput::IsKeyPressed(Key::Period);
            const bool f7 = FInput::IsKeyPressed(Key::F7);
            const bool f8 = FInput::IsKeyPressed(Key::F8);
            if ((comma && !bComma) || (f7 && !bF7))
                FGameplayDebugger::CycleSelectedAI(1);
            if ((period && !bPeriod) || (f8 && !bF8))
                FGameplayDebugger::CycleSelectedAI(-1);
            bComma = comma;
            bPeriod = period;
            bF7 = f7;
            bF8 = f8;
        }

        void DrawAITelemetry(UWorld* InWorld) {
            if (!InWorld || !FGameplayDebugger::ShowAI())
                return;

            int32_t bots = 0, activeBt = 0, moving = 0, searching = 0, combat = 0, reloading = 0, dead = 0;
            int32_t pathsValid = 0, pathsFailed = 0;
            for (AAIController* ai : InWorld->GetAIControllers()) {
                auto* bot = dynamic_cast<AShooterBotController*>(ai);
                if (!bot)
                    continue;
                ++bots;
                if (bot->IsBehaviorTreeRunning())
                    ++activeBt;
                switch (bot->GetBotState()) {
                case EShooterBotState::Search:
                    ++searching;
                    break;
                case EShooterBotState::Combat:
                case EShooterBotState::Aim:
                case EShooterBotState::Fire:
                case EShooterBotState::MoveToTarget:
                    ++combat;
                    break;
                case EShooterBotState::Reload:
                    ++reloading;
                    break;
                case EShooterBotState::Dead:
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
            std::snprintf(line, sizeof(line), "Paths valid:%d  failed:%d  [Shift+F5 AI] [, / F7 next] [. / F8 prev]",
                          pathsValid, pathsFailed);
            put(line);

            const auto& ais = InWorld->GetAIControllers();
            if (ais.empty())
                return;
            int32_t n = static_cast<int32_t>(ais.size());
            int32_t idx = FGameplayDebugger::GetSelectedAIIndex() % n;
            if (idx < 0)
                idx += n;
            auto* selected = dynamic_cast<AShooterBotController*>(ais[static_cast<size_t>(idx)]);
            if (!selected)
                return;
            auto* pawn = selected->GetPawn<AShooterCharacter>();
            auto* ps = dynamic_cast<AShooterPlayerState*>(selected->GetPlayerState());
            auto* tgt = selected->GetCurrentTarget();
            auto brain = selected->GetBrainComponent();
            auto board = selected->GetBlackboardComponent();
            glm::vec3 dest = selected->GetMoveDestination();
            glm::vec3 vel = pawn && pawn->GetCharacterMovement() ? pawn->GetCharacterMovement()->GetVelocity()
                                                                : glm::vec3(0.0f);
            std::snprintf(line, sizeof(line),
                          "SEL[%d] team=%s state=%s node=%s tgt=%s dist=%.1f los=%s hp=%.0f ammo=%d",
                          ps ? ps->GetPlayerId() : idx, ps ? ShooterTeamName(ps->GetTeam()) : "-",
                          ShooterBotStateName(selected->GetBotState()), brain ? brain->GetActiveNodeName().c_str() : "-",
                          tgt ? tgt->GetName().c_str() : "-",
                          board ? board->GetValueAsFloat("DistanceToTarget") : 0.0f,
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

            auto* gs = dynamic_cast<AShooterGameState*>(InWorld->GetGameState());
            auto* ch = InPC ? InPC->GetPawn<AShooterCharacter>() : nullptr;
            auto* ps = InPC ? dynamic_cast<AShooterPlayerState*>(InPC->GetPlayerState()) : nullptr;
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
                auto* bot = dynamic_cast<AShooterBotController*>(ai);
                if (!bot)
                    continue;
                auto* bps = bot->GetPlayerState();
                auto* tgt = bot->GetCurrentTarget();
                std::snprintf(line, sizeof(line), "BOT[%d] %s  tgt=%s", bps ? bps->GetPlayerId() : -1,
                              ShooterBotStateName(bot->GetBotState()), tgt ? tgt->GetName().c_str() : "-");
                put(line);
            }
        }
    } // namespace

    AShooterHUD::AShooterHUD(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AHUD(InHandle, InWorld, InName) {
        SetClass("AShooterHUD");
    }

    void AShooterHUD::BeginPlay() {
        AHUD::BeginPlay();
        if (!PlayerController)
            return;
        MenuWidget = UUserWidget::CreateWidget<UShooterMainMenuWidget>(PlayerController);
        LobbyWidget = UUserWidget::CreateWidget<UShooterLobbyWidget>(PlayerController);
        HudWidget = UUserWidget::CreateWidget<UShooterHUDWidget>(PlayerController);
        ScoreboardWidget = UUserWidget::CreateWidget<UShooterScoreboardWidget>(PlayerController);
        EndWidget = UUserWidget::CreateWidget<UShooterMatchEndWidget>(PlayerController);

        MenuWidget->AddToViewport(10);
        LobbyWidget->AddToViewport(10);
        HudWidget->AddToViewport(1);
        ScoreboardWidget->AddToViewport(15);
        EndWidget->AddToViewport(20);

        LobbyWidget->SetVisibility(ESlateVisibility::Collapsed);
        HudWidget->SetVisibility(ESlateVisibility::Collapsed);
        ScoreboardWidget->SetVisibility(ESlateVisibility::Collapsed);
        EndWidget->SetVisibility(ESlateVisibility::Collapsed);
        ShownState = EShooterMatchState::MainMenu;
    }

    void AShooterHUD::SyncWidgets() {
        auto* gs = World ? dynamic_cast<AShooterGameState*>(World->GetGameState()) : nullptr;
        EShooterMatchState state = gs ? gs->GetMatchState() : EShooterMatchState::MainMenu;
        ShownState = state;

        auto show = [](const TRef<UUserWidget>& w, bool bOn, bool bHit = true) {
            if (!w)
                return;
            if (!bOn)
                w->SetVisibility(ESlateVisibility::Collapsed);
            else
                w->SetVisibility(bHit ? ESlateVisibility::Visible : ESlateVisibility::HitTestInvisible);
        };
        show(MenuWidget, state == EShooterMatchState::MainMenu);
        show(LobbyWidget, state == EShooterMatchState::Lobby);
        show(HudWidget, state == EShooterMatchState::Playing || state == EShooterMatchState::Starting, false);
        show(EndWidget, state == EShooterMatchState::Finished);
    }

    void AShooterHUD::Tick(float DeltaSeconds) {
        AHUD::Tick(DeltaSeconds);
        SyncWidgets();

        auto* spc = dynamic_cast<AShooterPlayerController*>(PlayerController);
        const bool bWantSb = spc && spc->IsScoreboardHeld() &&
                             (ShownState == EShooterMatchState::Playing || ShownState == EShooterMatchState::Starting);
        if (ScoreboardWidget) {
            ScoreboardWidget->SetVisibility(bWantSb ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
            bScoreboardVisible = bWantSb;
        }

        DrawGameplayDebugOverlay(World, PlayerController);
    }

} // namespace Leon
