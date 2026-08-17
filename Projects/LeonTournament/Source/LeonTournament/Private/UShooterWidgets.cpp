#include "UShooterWidgets.hpp"
#include "AShooterGameMode.hpp"
#include "AShooterGameState.hpp"
#include "AShooterPlayerController.hpp"
#include "AShooterCharacter.hpp"
#include "AShooterPlayerState.hpp"
#include "AShooterWeapon.hpp"
#include "UShooterGameInstance.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Gameplay/UGameplayStatics.hpp"
#include "Engine/UEngine.hpp"
#include "Engine/UWorld.hpp"
#include "Core/FApplication.hpp"
#include "UMG/FUIRenderer.hpp"

#include <algorithm>
#include <cstdio>
#include <sstream>

namespace Leon {

    namespace {
        UShooterGameInstance* GI() {
            return UEngine::HasInstance() ? dynamic_cast<UShooterGameInstance*>(UEngine::Get().GetGameInstance().get())
                                          : nullptr;
        }

        AShooterGameMode* GM(APlayerController* InPC) {
            UWorld* world = InPC ? InPC->GetWorld() : nullptr;
            return world ? dynamic_cast<AShooterGameMode*>(world->GetGameMode()) : nullptr;
        }

        AShooterGameState* GS(APlayerController* InPC) {
            UWorld* world = InPC ? InPC->GetWorld() : nullptr;
            return world ? dynamic_cast<AShooterGameState*>(world->GetGameState()) : nullptr;
        }

        TRef<UButton> MakeButton(const std::string& InName, const std::string& InLabel, float InFont = 1.0f) {
            auto btn = std::make_shared<UButton>(InName);
            btn->SetNormalColor({0.12f, 0.18f, 0.30f, 0.95f});
            btn->SetHoveredColor({0.18f, 0.30f, 0.50f, 1.0f});
            btn->SetPressedColor({0.08f, 0.12f, 0.20f, 1.0f});
            auto label = std::make_shared<UTextBlock>(InName + "Label");
            label->SetText(InLabel);
            label->SetFontScale(InFont);
            label->SetColor({0.95f, 0.97f, 1.0f, 1.0f});
            label->SetJustification(ETextAlignment::Center);
            btn->SetContent(label);
            return btn;
        }
    } // namespace

    UShooterMainMenuWidget::UShooterMainMenuWidget(const std::string& InName) : UUserWidget(InName) {}
    void UShooterMainMenuWidget::Construct() {
        Build();
    }

    void UShooterMainMenuWidget::Build() {
        FUIRenderer::Init();
        Root = std::make_shared<UCanvasPanel>("MenuRoot");
        Root->SetSize({1280, 720});
        Root->SetBackgroundColor({0.02f, 0.03f, 0.06f, 0.92f});

        auto title = std::make_shared<UTextBlock>("Title");
        title->SetText("LEON TOURNAMENT");
        title->SetFontScale(1.6f);
        title->SetColor({0.90f, 0.93f, 1.0f, 1.0f});
        Root->AddChild(title, FAnchors::TopLeft(), FMargin(80, 80, -900, -130));

        auto sub = std::make_shared<UTextBlock>("Sub");
        sub->SetText("8v8 Team Deathmatch");
        sub->SetFontScale(0.9f);
        sub->SetColor({0.65f, 0.72f, 0.85f, 1.0f});
        Root->AddChild(sub, FAnchors::TopLeft(), FMargin(80, 130, -700, -160));

        auto offline = MakeButton("Offline", "PLAY OFFLINE");
        offline->OnClicked.AddLambda([this]() { OnOffline(); });
        Root->AddChild(offline, FAnchors::TopLeft(), FMargin(80, 220, -360, -268));

        auto host = MakeButton("Host", "HOST LAN");
        host->OnClicked.AddLambda([this]() { OnHostLan(); });
        Root->AddChild(host, FAnchors::TopLeft(), FMargin(80, 280, -360, -328));

        auto join = MakeButton("Join", "JOIN LAN");
        join->OnClicked.AddLambda([this]() { OnJoinLan(); });
        Root->AddChild(join, FAnchors::TopLeft(), FMargin(80, 340, -360, -388));

        auto ipLabel = std::make_shared<UTextBlock>("IpLabel");
        ipLabel->SetText("IP Address");
        ipLabel->SetFontScale(0.75f);
        ipLabel->SetColor({0.65f, 0.72f, 0.85f, 1.0f});
        Root->AddChild(ipLabel, FAnchors::TopLeft(), FMargin(80, 400, -360, -424));

        AddressField = std::make_shared<UEditableText>("JoinAddress");
        auto* session = GI();
        AddressField->SetText(session && !session->GetJoinAddress().empty() ? session->GetJoinAddress()
                                                                            : std::string("127.0.0.1"));
        AddressField->SetHint("127.0.0.1");
        AddressField->OnTextChanged = [](const std::string& InText) {
            if (auto* inst = GI())
                inst->SetJoinAddress(InText.empty() ? "127.0.0.1" : InText);
        };
        Root->AddChild(AddressField, FAnchors::TopLeft(), FMargin(80, 428, -360, -468));

        auto quit = MakeButton("Quit", "QUIT");
        quit->OnClicked.AddLambda([this]() { OnQuit(); });
        Root->AddChild(quit, FAnchors::TopLeft(), FMargin(80, 488, -360, -536));

        SetWidgetTree(Root);
        SetSize({1280, 720});
    }

    void UShooterMainMenuWidget::OnOffline() {
        if (auto* gi = GI())
            gi->SetSessionMode(EShooterSessionMode::Offline);
        if (auto* gm = GM(OwningPlayer))
            gm->EnterLobby();
    }

    void UShooterMainMenuWidget::OnHostLan() {
        if (auto* gi = GI()) {
            if (OwningPlayer && OwningPlayer->GetWorld())
                gi->HostLan(OwningPlayer->GetWorld());
        }
        if (auto* gm = GM(OwningPlayer))
            gm->EnterLobby();
    }

    void UShooterMainMenuWidget::OnJoinLan() {
        if (auto* gi = GI()) {
            if (OwningPlayer && OwningPlayer->GetWorld())
                gi->JoinLan(OwningPlayer->GetWorld(), gi->GetJoinAddress());
        }
    }

    void UShooterMainMenuWidget::OnQuit() {
        if (FApplication::HasInstance())
            FApplication::Get().Close();
    }

    UShooterLobbyWidget::UShooterLobbyWidget(const std::string& InName) : UUserWidget(InName) {}
    void UShooterLobbyWidget::Construct() {
        Build();
    }

    void UShooterLobbyWidget::Build() {
        FUIRenderer::Init();
        Root = std::make_shared<UCanvasPanel>("LobbyRoot");
        Root->SetSize({1280, 720});
        Root->SetBackgroundColor({0.03f, 0.04f, 0.08f, 0.90f});

        auto title = std::make_shared<UTextBlock>("LobbyTitle");
        title->SetText("LOBBY  —  8v8");
        title->SetFontScale(1.3f);
        Root->AddChild(title, FAnchors::TopLeft(), FMargin(60, 40, -500, -80));

        RosterText = std::make_shared<UTextBlock>("Roster");
        RosterText->SetFontScale(0.75f);
        Root->AddChild(RosterText, FAnchors::TopLeft(), FMargin(60, 100, -60, -520));

        auto start = MakeButton("Start", "START MATCH");
        start->OnClicked.AddLambda([this]() { OnStart(); });
        Root->AddChild(start, FAnchors::TopLeft(), FMargin(60, 560, -340, -608));

        auto back = MakeButton("Back", "BACK");
        back->OnClicked.AddLambda([this]() { OnBack(); });
        Root->AddChild(back, FAnchors::TopLeft(), FMargin(360, 560, -640, -608));

        SetWidgetTree(Root);
        SetSize({1280, 720});
    }

    void UShooterLobbyWidget::Tick(float InDeltaTime) {
        UUserWidget::Tick(InDeltaTime);
        auto* gs = GS(OwningPlayer);
        if (!gs || !RosterText)
            return;
        std::ostringstream ss;
        ss << "TEAM 1 (" << gs->GetTeam1PlayerCount() << "/8)\n";
        for (APlayerState* ps : gs->GetPlayerArray()) {
            auto* sps = dynamic_cast<AShooterPlayerState*>(ps);
            if (sps && sps->GetTeam() == EShooterTeam::Team1)
                ss << "  " << sps->GetPlayerName() << (sps->IsBot() ? "  [BOT]" : "") << "\n";
        }
        ss << "\nTEAM 2 (" << gs->GetTeam2PlayerCount() << "/8)\n";
        for (APlayerState* ps : gs->GetPlayerArray()) {
            auto* sps = dynamic_cast<AShooterPlayerState*>(ps);
            if (sps && sps->GetTeam() == EShooterTeam::Team2)
                ss << "  " << sps->GetPlayerName() << (sps->IsBot() ? "  [BOT]" : "") << "\n";
        }
        RosterText->SetText(ss.str());
    }

    void UShooterLobbyWidget::OnStart() {
        UWorld* world = OwningPlayer ? OwningPlayer->GetWorld() : nullptr;
        if (world && world->GetNetMode() == ENetMode::Client)
            return;
        if (auto* gm = GM(OwningPlayer))
            gm->RequestStartMatch();
    }

    void UShooterLobbyWidget::OnBack() {
        if (auto* gm = GM(OwningPlayer))
            gm->ReturnToMenu();
    }

    UShooterHUDWidget::UShooterHUDWidget(const std::string& InName) : UUserWidget(InName) {}
    void UShooterHUDWidget::Construct() {
        Build();
    }

    void UShooterHUDWidget::Build() {
        FUIRenderer::Init();
        Root = std::make_shared<UCanvasPanel>("HudRoot");
        Root->SetSize({1280, 720});

        Team1Text = std::make_shared<UTextBlock>("T1");
        Team1Text->SetFontScale(1.1f);
        Team1Text->SetColor({0.95f, 0.45f, 0.35f, 1.0f});
        Root->AddChild(Team1Text, FAnchors::TopLeft(), FMargin(40, 20, -400, -50));

        Team2Text = std::make_shared<UTextBlock>("T2");
        Team2Text->SetFontScale(1.1f);
        Team2Text->SetColor({0.35f, 0.55f, 0.95f, 1.0f});
        Root->AddChild(Team2Text, FAnchors::TopLeft(), FMargin(40, 50, -400, -80));

        TimerText = std::make_shared<UTextBlock>("Timer");
        TimerText->SetFontScale(1.0f);
        Root->AddChild(TimerText, FAnchors::TopRight(), FMargin(-180, 20, 30, -50));

        auto crossH = std::make_shared<UImage>("CrossH");
        crossH->SetTintColor({1, 1, 1, 0.95f});
        Root->AddChild(crossH, FAnchors::Center(), FMargin(-12.0f, -1.5f, -12.0f, -1.5f));
        auto crossV = std::make_shared<UImage>("CrossV");
        crossV->SetTintColor({1, 1, 1, 0.95f});
        Root->AddChild(crossV, FAnchors::Center(), FMargin(-1.5f, -12.0f, -1.5f, -12.0f));
        CrossH = crossH;
        CrossV = crossV;

        HealthText = std::make_shared<UTextBlock>("HP");
        HealthText->SetFontScale(1.0f);
        HealthText->SetColor({0.4f, 0.95f, 0.5f, 1.0f});
        Root->AddChild(HealthText, FAnchors::TopLeft(), FMargin(40, 660, -300, -700));

        AmmoText = std::make_shared<UTextBlock>("Ammo");
        AmmoText->SetFontScale(1.0f);
        Root->AddChild(AmmoText, FAnchors::TopRight(), FMargin(-220, 660, 40, -700));

        SprintText = std::make_shared<UTextBlock>("Sprint");
        SprintText->SetFontScale(0.7f);
        SprintText->SetColor({0.85f, 0.85f, 0.4f, 1.0f});
        Root->AddChild(SprintText, FAnchors::TopLeft(), FMargin(40, 630, -220, -656));

        HitMarkText = std::make_shared<UTextBlock>("HitMark");
        HitMarkText->SetText("X");
        HitMarkText->SetFontScale(1.4f);
        HitMarkText->SetColor({1.0f, 0.85f, 0.2f, 1.0f});
        HitMarkText->SetJustification(ETextAlignment::Center);
        Root->AddChild(HitMarkText, FAnchors::Center(), FMargin(-16.0f, -16.0f, -16.0f, -16.0f));
        HitMarkText->SetVisibility(ESlateVisibility::Collapsed);

        KillText = std::make_shared<UTextBlock>("KillConfirm");
        KillText->SetText("ELIMINATED");
        KillText->SetFontScale(1.2f);
        KillText->SetColor({1.0f, 0.75f, 0.2f, 1.0f});
        KillText->SetJustification(ETextAlignment::Center);
        Root->AddChild(KillText, FAnchors::Center(), FMargin(-120.0f, -80.0f, -120.0f, -40.0f));
        KillText->SetVisibility(ESlateVisibility::Collapsed);

        DamageFlash = std::make_shared<UImage>("DamageFlash");
        DamageFlash->SetTintColor({0.85f, 0.08f, 0.08f, 0.22f});
        Root->AddChild(DamageFlash, FAnchors::Fill(), FMargin(0, 0, 0, 0));
        DamageFlash->SetVisibility(ESlateVisibility::Collapsed);

        SetWidgetTree(Root);
        SetSize({1280, 720});
        SetVisibility(ESlateVisibility::HitTestInvisible);
    }

    void UShooterHUDWidget::Tick(float InDeltaTime) {
        UUserWidget::Tick(InDeltaTime);
        auto* gs = GS(OwningPlayer);
        if (gs && Team1Text && Team2Text && TimerText) {
            Team1Text->SetText(std::string("TEAM 1    ") + std::to_string(gs->GetTeam1Kills()));
            Team2Text->SetText(std::string("TEAM 2    ") + std::to_string(gs->GetTeam2Kills()));
            const float seconds = gs->GetMatchState() == EShooterMatchState::Starting ? gs->GetCountdownRemaining()
                                                                                      : gs->GetRemainingTime();
            int t = static_cast<int>(std::max(0.0f, seconds));
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%02d:%02d", t / 60, t % 60);
            TimerText->SetText(buf);
        }

        AShooterCharacter* ch = OwningPlayer ? OwningPlayer->GetPawn<AShooterCharacter>() : nullptr;
        auto* health = ch ? ch->GetHealthComponent().get() : nullptr;
        if (HealthText) {
            if (!ch || !health)
                HealthText->SetText("0 HP");
            else if (health->IsDead())
                HealthText->SetText("DEAD");
            else
                HealthText->SetText(std::to_string(static_cast<int>(health->GetHealth())) + " HP");
        }
        if (AmmoText) {
            if (!ch || !ch->GetWeapon()) {
                AmmoText->SetText("0 / 0");
            } else if (ch->GetWeapon()->IsReloading()) {
                AmmoText->SetText("RELOADING");
            } else {
                AmmoText->SetText(std::to_string(ch->GetWeapon()->GetCurrentAmmo()) + " / " +
                                  std::to_string(ch->GetWeapon()->GetMagazineSize()));
            }
        }
        if (SprintText)
            SprintText->SetText(ch && ch->IsSprinting() ? "SPRINT" : "");

        auto* spc = dynamic_cast<AShooterPlayerController*>(OwningPlayer);
        if (HitMarkText)
            HitMarkText->SetVisibility(spc && spc->IsHitMarkerActive() ? ESlateVisibility::HitTestInvisible
                                                                       : ESlateVisibility::Collapsed);
        if (KillText)
            KillText->SetVisibility(spc && spc->IsKillConfirmActive() ? ESlateVisibility::HitTestInvisible
                                                                      : ESlateVisibility::Collapsed);
        if (DamageFlash)
            DamageFlash->SetVisibility(spc && spc->IsDamageFlashActive() ? ESlateVisibility::HitTestInvisible
                                                                         : ESlateVisibility::Collapsed);
    }

    UShooterScoreboardWidget::UShooterScoreboardWidget(const std::string& InName) : UUserWidget(InName) {}
    void UShooterScoreboardWidget::Construct() {
        Build();
    }

    void UShooterScoreboardWidget::Build() {
        FUIRenderer::Init();
        Root = std::make_shared<UCanvasPanel>("SBRoot");
        Root->SetSize({1280, 720});
        Root->SetBackgroundColor({0.02f, 0.02f, 0.04f, 0.82f});
        auto title = std::make_shared<UTextBlock>("SBTitle");
        title->SetText("PLAYER                 TEAM      K    D    A");
        title->SetFontScale(0.85f);
        Root->AddChild(title, FAnchors::TopLeft(), FMargin(120, 80, -120, -110));
        RowsText = std::make_shared<UTextBlock>("SBRows");
        RowsText->SetFontScale(0.75f);
        Root->AddChild(RowsText, FAnchors::TopLeft(), FMargin(120, 120, -120, -80));
        SetWidgetTree(Root);
        SetSize({1280, 720});
        SetVisibility(ESlateVisibility::HitTestInvisible);
    }

    void UShooterScoreboardWidget::Tick(float InDeltaTime) {
        UUserWidget::Tick(InDeltaTime);
        auto* gs = GS(OwningPlayer);
        if (!gs || !RowsText)
            return;
        std::ostringstream ss;
        for (auto* ps : gs->GetSortedScoreboard()) {
            char line[128];
            std::snprintf(line, sizeof(line), "%-20s  %-8s  %3d  %3d  %3d\n", ps->GetPlayerName().c_str(),
                          ShooterTeamName(ps->GetTeam()), ps->GetKills(), ps->GetDeaths(), ps->GetAssists());
            ss << line;
        }
        RowsText->SetText(ss.str());
    }

    UShooterMatchEndWidget::UShooterMatchEndWidget(const std::string& InName) : UUserWidget(InName) {}
    void UShooterMatchEndWidget::Construct() {
        Build();
    }

    void UShooterMatchEndWidget::Build() {
        FUIRenderer::Init();
        Root = std::make_shared<UCanvasPanel>("EndRoot");
        Root->SetSize({1280, 720});
        Root->SetBackgroundColor({0.02f, 0.02f, 0.05f, 0.92f});
        ResultText = std::make_shared<UTextBlock>("Result");
        ResultText->SetFontScale(2.0f);
        Root->AddChild(ResultText, FAnchors::TopLeft(), FMargin(80, 80, -80, -160));
        StatsText = std::make_shared<UTextBlock>("Stats");
        StatsText->SetFontScale(1.0f);
        Root->AddChild(StatsText, FAnchors::TopLeft(), FMargin(80, 200, -80, -400));
        auto back = MakeButton("Return", "RETURN TO MENU");
        back->OnClicked.AddLambda([this]() { OnReturn(); });
        Root->AddChild(back, FAnchors::TopLeft(), FMargin(80, 520, -400, -568));
        SetWidgetTree(Root);
        SetSize({1280, 720});
    }

    void UShooterMatchEndWidget::Tick(float InDeltaTime) {
        UUserWidget::Tick(InDeltaTime);
        auto* gs = GS(OwningPlayer);
        if (!gs || !ResultText)
            return;
        EShooterTeam localTeam = EShooterTeam::None;
        if (OwningPlayer) {
            if (auto* ps = dynamic_cast<AShooterPlayerState*>(OwningPlayer->GetPlayerState()))
                localTeam = ps->GetTeam();
        }
        std::string result = "DRAW";
        if (gs->GetMatchWinner() == EShooterMatchWinner::Team1)
            result = localTeam == EShooterTeam::Team1 ? "WIN" : "LOSS";
        else if (gs->GetMatchWinner() == EShooterMatchWinner::Team2)
            result = localTeam == EShooterTeam::Team2 ? "WIN" : "LOSS";
        else if (gs->GetMatchWinner() == EShooterMatchWinner::Draw)
            result = "DRAW";
        ResultText->SetText(result);

        std::ostringstream ss;
        ss << "TEAM 1  " << gs->GetTeam1Kills() << "     TEAM 2  " << gs->GetTeam2Kills() << "\n";
        if (OwningPlayer) {
            if (auto* ps = dynamic_cast<AShooterPlayerState*>(OwningPlayer->GetPlayerState())) {
                ss << "\nYOU  K " << ps->GetKills() << "  D " << ps->GetDeaths() << "  A " << ps->GetAssists();
            }
        }
        if (StatsText)
            StatsText->SetText(ss.str());
    }

    void UShooterMatchEndWidget::OnReturn() {
        if (auto* gm = GM(OwningPlayer))
            gm->ReturnToMenu();
    }

} // namespace Leon
