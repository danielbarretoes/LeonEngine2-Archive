#include "ULeonTournamentWidgets.hpp"
#include "ALeonTournamentAnimLabGameMode.hpp"
#include "ALeonTournamentGameMode.hpp"
#include "ALeonTournamentGameState.hpp"
#include "ALeonTournamentPlayerController.hpp"
#include "ALeonTournamentCharacter.hpp"
#include "ALeonTournamentPlayerState.hpp"
#include "ALeonTournamentWeapon.hpp"
#include "ULeonTournamentGameInstance.hpp"
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
        ULeonTournamentGameInstance* GI() {
            return UEngine::HasInstance()
                       ? dynamic_cast<ULeonTournamentGameInstance*>(UEngine::Get().GetGameInstance().get())
                       : nullptr;
        }

        ALeonTournamentGameMode* GM(APlayerController* InPC) {
            UWorld* world = InPC ? InPC->GetWorld() : nullptr;
            return world ? dynamic_cast<ALeonTournamentGameMode*>(world->GetGameMode()) : nullptr;
        }

        ALeonTournamentGameState* GS(APlayerController* InPC) {
            UWorld* world = InPC ? InPC->GetWorld() : nullptr;
            return world ? dynamic_cast<ALeonTournamentGameState*>(world->GetGameState()) : nullptr;
        }

        bool IsClientWorld(APlayerController* InPC) {
            UWorld* world = InPC ? InPC->GetWorld() : nullptr;
            return world && world->GetNetMode() == ENetMode::Client;
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

    ULeonTournamentMainMenuWidget::ULeonTournamentMainMenuWidget(const std::string& InName) : UUserWidget(InName) {}
    void ULeonTournamentMainMenuWidget::Construct() {
        Build();
    }

    void ULeonTournamentMainMenuWidget::Build() {
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
        sub->SetText("2v2 Team Deathmatch");
        sub->SetFontScale(0.9f);
        sub->SetColor({0.65f, 0.72f, 0.85f, 1.0f});
        Root->AddChild(sub, FAnchors::TopLeft(), FMargin(80, 130, -700, -160));

        auto offline = MakeButton("Offline", "PLAY OFFLINE");
        offline->OnClicked.AddLambda([this]() { OnOffline(); });
        Root->AddChild(offline, FAnchors::TopLeft(), FMargin(80, 220, -360, -268));

        auto animLab = MakeButton("AnimLab", "ANIM LAB");
        animLab->OnClicked.AddLambda([this]() { OnAnimLab(); });
        Root->AddChild(animLab, FAnchors::TopLeft(), FMargin(80, 280, -360, -328));

        auto host = MakeButton("Host", "HOST LAN");
        host->OnClicked.AddLambda([this]() { OnHostLan(); });
        Root->AddChild(host, FAnchors::TopLeft(), FMargin(80, 340, -360, -388));

        auto join = MakeButton("Join", "JOIN LAN");
        join->OnClicked.AddLambda([this]() { OnJoinLan(); });
        Root->AddChild(join, FAnchors::TopLeft(), FMargin(80, 400, -360, -448));

        auto ipLabel = std::make_shared<UTextBlock>("IpLabel");
        ipLabel->SetText("IP Address");
        ipLabel->SetFontScale(0.75f);
        ipLabel->SetColor({0.65f, 0.72f, 0.85f, 1.0f});
        Root->AddChild(ipLabel, FAnchors::TopLeft(), FMargin(80, 460, -360, -484));

        AddressField = std::make_shared<UEditableText>("JoinAddress");
        auto* session = GI();
        AddressField->SetText(session && !session->GetJoinAddress().empty() ? session->GetJoinAddress()
                                                                            : std::string("127.0.0.1"));
        AddressField->SetHint("127.0.0.1");
        AddressField->OnTextChanged = [](const std::string& InText) {
            if (auto* inst = GI())
                inst->SetJoinAddress(InText.empty() ? "127.0.0.1" : InText);
        };
        Root->AddChild(AddressField, FAnchors::TopLeft(), FMargin(80, 488, -360, -528));

        auto quit = MakeButton("Quit", "QUIT");
        quit->OnClicked.AddLambda([this]() { OnQuit(); });
        Root->AddChild(quit, FAnchors::TopLeft(), FMargin(80, 548, -360, -596));

        SetWidgetTree(Root);
        SetSize({1280, 720});
    }

    void ULeonTournamentMainMenuWidget::OnOffline() {
        if (IsClientWorld(OwningPlayer))
            return;
        if (auto* gi = GI())
            gi->SetSessionMode(ELeonTournamentSessionMode::Offline);
        if (auto* gm = GM(OwningPlayer))
            gm->EnterLobby();
    }

    void ULeonTournamentMainMenuWidget::OnAnimLab() {
        if (IsClientWorld(OwningPlayer))
            return;
        if (auto* gm = GM(OwningPlayer))
            gm->OpenAnimLab();
    }

    void ULeonTournamentMainMenuWidget::OnHostLan() {
        if (auto* gi = GI()) {
            if (OwningPlayer && OwningPlayer->GetWorld())
                gi->HostLan(OwningPlayer->GetWorld());
        }
        if (auto* gm = GM(OwningPlayer))
            gm->EnterLobby();
    }

    void ULeonTournamentMainMenuWidget::OnJoinLan() {
        if (auto* gi = GI()) {
            if (OwningPlayer && OwningPlayer->GetWorld())
                gi->JoinLan(OwningPlayer->GetWorld(), gi->GetJoinAddress());
        }
    }

    void ULeonTournamentMainMenuWidget::OnQuit() {
        if (FApplication::HasInstance())
            FApplication::Get().Close();
    }

    ULeonTournamentLobbyWidget::ULeonTournamentLobbyWidget(const std::string& InName) : UUserWidget(InName) {}
    void ULeonTournamentLobbyWidget::Construct() {
        Build();
    }

    void ULeonTournamentLobbyWidget::Build() {
        FUIRenderer::Init();
        Root = std::make_shared<UCanvasPanel>("LobbyRoot");
        Root->SetSize({1280, 720});
        Root->SetBackgroundColor({0.03f, 0.04f, 0.08f, 0.90f});

        auto title = std::make_shared<UTextBlock>("LobbyTitle");
        title->SetText("LOBBY  —  2v2");
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

    void ULeonTournamentLobbyWidget::Tick(float InDeltaTime) {
        UUserWidget::Tick(InDeltaTime);
        auto* gs = GS(OwningPlayer);
        if (!gs || !RosterText)
            return;
        std::ostringstream ss;
        ss << "TEAM 1 (" << gs->GetTeam1PlayerCount() << "/2)\n";
        for (APlayerState* ps : gs->GetPlayerArray()) {
            auto* sps = dynamic_cast<ALeonTournamentPlayerState*>(ps);
            if (sps && sps->GetTeam() == ELeonTournamentTeam::Team1)
                ss << "  " << sps->GetPlayerName() << (sps->IsBot() ? "  [BOT]" : "") << "\n";
        }
        ss << "\nTEAM 2 (" << gs->GetTeam2PlayerCount() << "/2)\n";
        for (APlayerState* ps : gs->GetPlayerArray()) {
            auto* sps = dynamic_cast<ALeonTournamentPlayerState*>(ps);
            if (sps && sps->GetTeam() == ELeonTournamentTeam::Team2)
                ss << "  " << sps->GetPlayerName() << (sps->IsBot() ? "  [BOT]" : "") << "\n";
        }
        RosterText->SetText(ss.str());
    }

    void ULeonTournamentLobbyWidget::OnStart() {
        UWorld* world = OwningPlayer ? OwningPlayer->GetWorld() : nullptr;
        if (world && world->GetNetMode() == ENetMode::Client)
            return;
        if (auto* gm = GM(OwningPlayer))
            gm->RequestStartMatch();
    }

    void ULeonTournamentLobbyWidget::OnBack() {
        if (IsClientWorld(OwningPlayer))
            return;
        if (auto* gm = GM(OwningPlayer))
            gm->ReturnToMenu();
    }

    ULeonTournamentHUDWidget::ULeonTournamentHUDWidget(const std::string& InName) : UUserWidget(InName) {}
    void ULeonTournamentHUDWidget::Construct() {
        Build();
    }

    void ULeonTournamentHUDWidget::Build() {
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

        CrosshairText = std::make_shared<UTextBlock>("Crosshair");
        CrosshairText->SetText("+");
        CrosshairText->SetFontScale(1.8f);
        CrosshairText->SetColor({0.95f, 0.97f, 1.0f, 0.95f});
        CrosshairText->SetJustification(ETextAlignment::Center);
        Root->AddChild(CrosshairText, FAnchors::Center(), FMargin(-28.0f, -28.0f, -28.0f, -28.0f));

        HealthText = std::make_shared<UTextBlock>("HP");
        HealthText->SetFontScale(1.0f);
        HealthText->SetColor({0.4f, 0.95f, 0.5f, 1.0f});
        Root->AddChild(HealthText, FAnchors::TopLeft(), FMargin(40, 660, -300, -700));

        AmmoText = std::make_shared<UTextBlock>("Ammo");
        AmmoText->SetFontScale(1.0f);
        Root->AddChild(AmmoText, FAnchors::TopRight(), FMargin(-220, 660, 40, -700));

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

        HintText = std::make_shared<UTextBlock>("Hints");
        HintText->SetFontScale(0.75f);
        HintText->SetColor({0.75f, 0.82f, 0.92f, 0.9f});
        HintText->SetVisibility(ESlateVisibility::Collapsed);
        Root->AddChild(HintText, FAnchors::TopLeft(), FMargin(40, 90, -520, -160));

        SetWidgetTree(Root);
        SetSize({1280, 720});
        SetVisibility(ESlateVisibility::HitTestInvisible);
    }

    void ULeonTournamentHUDWidget::Tick(float InDeltaTime) {
        UUserWidget::Tick(InDeltaTime);
        auto* gs = GS(OwningPlayer);
        UWorld* world = OwningPlayer ? OwningPlayer->GetWorld() : nullptr;
        auto* animLab = world ? dynamic_cast<ALeonTournamentAnimLabGameMode*>(world->GetGameMode()) : nullptr;
        const bool bLab = animLab != nullptr;

        if (Team1Text && Team2Text && TimerText) {
            if (bLab) {
                Team1Text->SetText("ANIM LAB");
                Team2Text->SetText(animLab->PrefersThirdPerson() ? "CAMERA  3RD" : "CAMERA  1ST");
                TimerText->SetText("ESC MENU");
            } else if (gs) {
                Team1Text->SetText(std::string("TEAM 1    ") + std::to_string(gs->GetTeam1Kills()));
                Team2Text->SetText(std::string("TEAM 2    ") + std::to_string(gs->GetTeam2Kills()));
                const float seconds = gs->GetMatchState() == ELeonTournamentMatchState::Starting
                                          ? gs->GetCountdownRemaining()
                                          : gs->GetRemainingTime();
                int t = static_cast<int>(std::max(0.0f, seconds));
                char buf[16];
                std::snprintf(buf, sizeof(buf), "%02d:%02d", t / 60, t % 60);
                TimerText->SetText(buf);
            }
        }

        ALeonTournamentCharacter* ch = OwningPlayer ? OwningPlayer->GetPawn<ALeonTournamentCharacter>() : nullptr;
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
            if (bLab && ch && !ch->GetWeapon()) {
                AmmoText->SetText("MELEE LAB");
            } else if (!ch || !ch->GetWeapon()) {
                AmmoText->SetText("0 / 0");
            } else if (ch->GetWeapon()->IsReloading()) {
                AmmoText->SetText("RELOADING");
            } else {
                AmmoText->SetText(std::to_string(ch->GetWeapon()->GetCurrentAmmo()) + " / " +
                                  std::to_string(ch->GetWeapon()->GetMagazineSize()));
            }
        }
        if (HintText) {
            if (bLab) {
                HintText->SetVisibility(ESlateVisibility::HitTestInvisible);
                HintText->SetText("V = Camera   LMB = Fire   R = Reload   Shift+F1 = Debug");
            } else {
                HintText->SetVisibility(ESlateVisibility::Collapsed);
            }
        }
        auto* spc = dynamic_cast<ALeonTournamentPlayerController*>(OwningPlayer);
        if (CrosshairText) {
            const bool bHit = spc && spc->IsHitMarkerActive();
            const bool bShow = !bLab || (ch && !ch->IsThirdPerson());
            CrosshairText->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
            CrosshairText->SetText(bHit ? "X" : "+");
            CrosshairText->SetFontScale(bHit ? 2.2f : 1.8f);
            CrosshairText->SetColor(bHit ? glm::vec4(1.0f, 0.85f, 0.2f, 1.0f) : glm::vec4(0.95f, 0.97f, 1.0f, 0.95f));
        }
        if (KillText)
            KillText->SetVisibility(spc && spc->IsKillConfirmActive() ? ESlateVisibility::HitTestInvisible
                                                                      : ESlateVisibility::Collapsed);
        if (DamageFlash)
            DamageFlash->SetVisibility(spc && spc->IsDamageFlashActive() ? ESlateVisibility::HitTestInvisible
                                                                         : ESlateVisibility::Collapsed);
    }

    ULeonTournamentScoreboardWidget::ULeonTournamentScoreboardWidget(const std::string& InName) : UUserWidget(InName) {}
    void ULeonTournamentScoreboardWidget::Construct() {
        Build();
    }

    void ULeonTournamentScoreboardWidget::Build() {
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

    void ULeonTournamentScoreboardWidget::Tick(float InDeltaTime) {
        UUserWidget::Tick(InDeltaTime);
        auto* gs = GS(OwningPlayer);
        if (!gs || !RowsText)
            return;
        std::ostringstream ss;
        for (auto* ps : gs->GetSortedScoreboard()) {
            char line[128];
            std::snprintf(line, sizeof(line), "%-20s  %-8s  %3d  %3d  %3d\n", ps->GetPlayerName().c_str(),
                          LeonTournamentTeamName(ps->GetTeam()), ps->GetKills(), ps->GetDeaths(), ps->GetAssists());
            ss << line;
        }
        RowsText->SetText(ss.str());
    }

    ULeonTournamentMatchEndWidget::ULeonTournamentMatchEndWidget(const std::string& InName) : UUserWidget(InName) {}
    void ULeonTournamentMatchEndWidget::Construct() {
        Build();
    }

    void ULeonTournamentMatchEndWidget::Build() {
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

    void ULeonTournamentMatchEndWidget::Tick(float InDeltaTime) {
        UUserWidget::Tick(InDeltaTime);
        auto* gs = GS(OwningPlayer);
        if (!gs || !ResultText)
            return;
        ELeonTournamentTeam localTeam = ELeonTournamentTeam::None;
        if (OwningPlayer) {
            if (auto* ps = dynamic_cast<ALeonTournamentPlayerState*>(OwningPlayer->GetPlayerState()))
                localTeam = ps->GetTeam();
        }
        std::string result = "DRAW";
        if (gs->GetMatchWinner() == ELeonTournamentMatchWinner::Team1)
            result = localTeam == ELeonTournamentTeam::Team1 ? "WIN" : "LOSS";
        else if (gs->GetMatchWinner() == ELeonTournamentMatchWinner::Team2)
            result = localTeam == ELeonTournamentTeam::Team2 ? "WIN" : "LOSS";
        else if (gs->GetMatchWinner() == ELeonTournamentMatchWinner::Draw)
            result = "DRAW";
        ResultText->SetText(result);

        std::ostringstream ss;
        ss << "TEAM 1  " << gs->GetTeam1Kills() << "     TEAM 2  " << gs->GetTeam2Kills() << "\n";
        if (OwningPlayer) {
            if (auto* ps = dynamic_cast<ALeonTournamentPlayerState*>(OwningPlayer->GetPlayerState())) {
                ss << "\nYOU  K " << ps->GetKills() << "  D " << ps->GetDeaths() << "  A " << ps->GetAssists();
            }
        }
        if (StatsText)
            StatsText->SetText(ss.str());
    }

    void ULeonTournamentMatchEndWidget::OnReturn() {
        if (IsClientWorld(OwningPlayer))
            return;
        if (auto* gm = GM(OwningPlayer))
            gm->ReturnToMenu();
    }

} // namespace Leon
