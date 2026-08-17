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
#include "Core/FInput.hpp"
#include "Core/FInputSettings.hpp"
#include "UMG/FUIRenderer.hpp"
#include "Renderer/FPerspectiveCamera.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <glm/glm.hpp>

namespace Leon {

    namespace {
        ULeonTournamentGameInstance* GI() {
            return UEngine::HasInstance()
                       ? dynamic_cast<ULeonTournamentGameInstance*>(UEngine::Get().GetGameInstance().get())
                       : nullptr;
        }

        bool GamepadEdge(int InButton, bool& InOutWasDown) {
            const FInputSettings& input = FInputSettings::Get();
            if (!input.bEnableGamepad || !FInput::IsGamepadConnected(input.GamepadId)) {
                InOutWasDown = false;
                return false;
            }
            const bool down = FInput::IsGamepadButtonPressed(InButton, input.GamepadId);
            const bool edge = down && !InOutWasDown;
            InOutWasDown = down;
            return edge;
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

        auto charTitle = std::make_shared<UTextBlock>("CharTitle");
        charTitle->SetText("CHARACTER");
        charTitle->SetFontScale(0.75f);
        charTitle->SetColor({0.65f, 0.72f, 0.85f, 1.0f});
        Root->AddChild(charTitle, FAnchors::TopLeft(), FMargin(420, 220, -900, -248));

        auto prevChar = MakeButton("PrevChar", "<", 1.2f);
        prevChar->OnClicked.AddLambda([this]() { OnPrevCharacter(); });
        Root->AddChild(prevChar, FAnchors::TopLeft(), FMargin(420, 260, -480, -308));

        CharacterLabel = std::make_shared<UTextBlock>("CharName");
        CharacterLabel->SetFontScale(1.05f);
        CharacterLabel->SetColor({0.95f, 0.97f, 1.0f, 1.0f});
        CharacterLabel->SetJustification(ETextAlignment::Center);
        Root->AddChild(CharacterLabel, FAnchors::TopLeft(), FMargin(490, 268, -760, -300));
        RefreshCharacterLabel();

        auto nextChar = MakeButton("NextChar", ">", 1.2f);
        nextChar->OnClicked.AddLambda([this]() { OnNextCharacter(); });
        Root->AddChild(nextChar, FAnchors::TopLeft(), FMargin(770, 260, -830, -308));

        auto charHint = std::make_shared<UTextBlock>("CharHint");
        charHint->SetText("Same Mixamo skeleton — shared anims");
        charHint->SetFontScale(0.65f);
        charHint->SetColor({0.55f, 0.62f, 0.75f, 1.0f});
        Root->AddChild(charHint, FAnchors::TopLeft(), FMargin(420, 320, -900, -348));

        auto quit = MakeButton("Quit", "QUIT");
        quit->OnClicked.AddLambda([this]() { OnQuit(); });
        Root->AddChild(quit, FAnchors::TopLeft(), FMargin(80, 548, -360, -596));

        SetWidgetTree(Root);
        SetSize({1280, 720});
    }

    void ULeonTournamentMainMenuWidget::Tick(float InDeltaTime) {
        UUserWidget::Tick(InDeltaTime);
        RefreshCharacterLabel();
        // Xbox: A/Start = Offline, LB/RB = character
        if (GamepadEdge(GamepadButton::A, bPadAWasDown) || GamepadEdge(GamepadButton::Start, bPadStartWasDown))
            OnOffline();
        if (GamepadEdge(GamepadButton::LeftBumper, bPadLBWasDown))
            OnPrevCharacter();
        if (GamepadEdge(GamepadButton::RightBumper, bPadRBWasDown))
            OnNextCharacter();
    }

    void ULeonTournamentMainMenuWidget::RefreshCharacterLabel() {
        if (!CharacterLabel)
            return;
        auto* gi = GI();
        const auto skin = gi ? gi->GetSelectedCharacterSkin() : ELeonTournamentCharacterSkin::YBot;
        CharacterLabel->SetText(LeonTournamentCharacterSkinName(skin));
    }

    void ULeonTournamentMainMenuWidget::OnPrevCharacter() {
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_UIClick", 0.45f);
        if (auto* gi = GI())
            gi->CycleSelectedCharacterSkin(-1);
        RefreshCharacterLabel();
    }

    void ULeonTournamentMainMenuWidget::OnNextCharacter() {
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_UIClick", 0.45f);
        if (auto* gi = GI())
            gi->CycleSelectedCharacterSkin(1);
        RefreshCharacterLabel();
    }

    void ULeonTournamentMainMenuWidget::OnOffline() {
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_UIClick", 0.5f);
        if (IsClientWorld(OwningPlayer))
            return;
        if (auto* gi = GI())
            gi->SetSessionMode(ELeonTournamentSessionMode::Offline);
        if (auto* gm = GM(OwningPlayer))
            gm->EnterLobby();
    }

    void ULeonTournamentMainMenuWidget::OnAnimLab() {
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_UIClick", 0.5f);
        if (IsClientWorld(OwningPlayer))
            return;
        if (auto* gm = GM(OwningPlayer))
            gm->OpenAnimLab();
    }

    void ULeonTournamentMainMenuWidget::OnHostLan() {
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_UIClick", 0.5f);
        if (auto* gi = GI()) {
            if (OwningPlayer && OwningPlayer->GetWorld())
                gi->HostLan(OwningPlayer->GetWorld());
        }
        if (auto* gm = GM(OwningPlayer))
            gm->EnterLobby();
    }

    void ULeonTournamentMainMenuWidget::OnJoinLan() {
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_UIClick", 0.5f);
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

        TitleText = std::make_shared<UTextBlock>("LobbyTitle");
        TitleText->SetText("LOBBY  —  UP TO 12");
        TitleText->SetFontScale(1.3f);
        Root->AddChild(TitleText, FAnchors::TopLeft(), FMargin(60, 28, -520, -68));

        RosterText = std::make_shared<UTextBlock>("Roster");
        RosterText->SetFontScale(0.72f);
        Root->AddChild(RosterText, FAnchors::TopLeft(), FMargin(60, 78, -60, -420));

        auto botsTitle = std::make_shared<UTextBlock>("BotsTitle");
        botsTitle->SetText("BOTS PER TEAM");
        botsTitle->SetFontScale(0.8f);
        botsTitle->SetColor({0.65f, 0.72f, 0.85f, 1.0f});
        Root->AddChild(botsTitle, FAnchors::TopLeft(), FMargin(60, 430, -400, -458));

        auto t1Minus = MakeButton("T1BotsMinus", "-", 1.0f);
        t1Minus->OnClicked.AddLambda([this]() { OnAdjustBotsTeam1(-1); });
        Root->AddChild(t1Minus, FAnchors::TopLeft(), FMargin(60, 466, -110, -504));
        BotsTeam1Label = std::make_shared<UTextBlock>("T1BotsLabel");
        BotsTeam1Label->SetFontScale(0.95f);
        BotsTeam1Label->SetJustification(ETextAlignment::Center);
        Root->AddChild(BotsTeam1Label, FAnchors::TopLeft(), FMargin(120, 474, -360, -506));
        auto t1Plus = MakeButton("T1BotsPlus", "+", 1.0f);
        t1Plus->OnClicked.AddLambda([this]() { OnAdjustBotsTeam1(1); });
        Root->AddChild(t1Plus, FAnchors::TopLeft(), FMargin(370, 466, -420, -504));

        auto t2Minus = MakeButton("T2BotsMinus", "-", 1.0f);
        t2Minus->OnClicked.AddLambda([this]() { OnAdjustBotsTeam2(-1); });
        Root->AddChild(t2Minus, FAnchors::TopLeft(), FMargin(60, 514, -110, -552));
        BotsTeam2Label = std::make_shared<UTextBlock>("T2BotsLabel");
        BotsTeam2Label->SetFontScale(0.95f);
        BotsTeam2Label->SetJustification(ETextAlignment::Center);
        Root->AddChild(BotsTeam2Label, FAnchors::TopLeft(), FMargin(120, 522, -360, -554));
        auto t2Plus = MakeButton("T2BotsPlus", "+", 1.0f);
        t2Plus->OnClicked.AddLambda([this]() { OnAdjustBotsTeam2(1); });
        Root->AddChild(t2Plus, FAnchors::TopLeft(), FMargin(370, 514, -420, -552));

        CapacityHint = std::make_shared<UTextBlock>("CapacityHint");
        CapacityHint->SetFontScale(0.7f);
        CapacityHint->SetColor({0.7f, 0.78f, 0.9f, 1.0f});
        Root->AddChild(CapacityHint, FAnchors::TopLeft(), FMargin(60, 560, -520, -588));

        auto charTitle = std::make_shared<UTextBlock>("LobbyCharTitle");
        charTitle->SetText("YOUR CHARACTER");
        charTitle->SetFontScale(0.75f);
        charTitle->SetColor({0.65f, 0.72f, 0.85f, 1.0f});
        Root->AddChild(charTitle, FAnchors::TopLeft(), FMargin(620, 430, -1100, -458));

        auto prevChar = MakeButton("LobbyPrevChar", "<", 1.1f);
        prevChar->OnClicked.AddLambda([this]() { OnPrevCharacter(); });
        Root->AddChild(prevChar, FAnchors::TopLeft(), FMargin(620, 466, -680, -510));

        CharacterLabel = std::make_shared<UTextBlock>("LobbyCharName");
        CharacterLabel->SetFontScale(1.0f);
        CharacterLabel->SetColor({0.95f, 0.97f, 1.0f, 1.0f});
        CharacterLabel->SetJustification(ETextAlignment::Center);
        Root->AddChild(CharacterLabel, FAnchors::TopLeft(), FMargin(690, 474, -980, -506));
        RefreshCharacterLabel();

        auto nextChar = MakeButton("LobbyNextChar", ">", 1.1f);
        nextChar->OnClicked.AddLambda([this]() { OnNextCharacter(); });
        Root->AddChild(nextChar, FAnchors::TopLeft(), FMargin(990, 466, -1050, -510));

        auto start = MakeButton("Start", "START MATCH");
        start->OnClicked.AddLambda([this]() { OnStart(); });
        Root->AddChild(start, FAnchors::TopLeft(), FMargin(60, 600, -340, -648));

        auto back = MakeButton("Back", "BACK");
        back->OnClicked.AddLambda([this]() { OnBack(); });
        Root->AddChild(back, FAnchors::TopLeft(), FMargin(360, 600, -640, -648));

        RefreshBotLabels();
        SetWidgetTree(Root);
        SetSize({1280, 720});
    }

    void ULeonTournamentLobbyWidget::RefreshBotLabels() {
        auto* gi = GI();
        const int t1 = gi ? gi->GetDesiredBotsTeam1() : 0;
        const int t2 = gi ? gi->GetDesiredBotsTeam2() : 0;
        if (BotsTeam1Label)
            BotsTeam1Label->SetText("TEAM 1 BOTS: " + std::to_string(t1));
        if (BotsTeam2Label)
            BotsTeam2Label->SetText("TEAM 2 BOTS: " + std::to_string(t2));
        if (CapacityHint) {
            const int total = 1 + t1 + t2;
            CapacityHint->SetText("Capacity ~" + std::to_string(total) + " / 12  (you + bots)   D-Pad adjust");
        }
    }

    void ULeonTournamentLobbyWidget::OnAdjustBotsTeam1(int InDelta) {
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_UIClick", 0.4f);
        if (auto* gi = GI())
            gi->AdjustDesiredBotsTeam1(InDelta);
        RefreshBotLabels();
        // Only spawn bots while the lobby is active — never during MainMenu widget construct.
        if (auto* gs = GS(OwningPlayer);
            gs && gs->GetMatchState() == ELeonTournamentMatchState::Lobby) {
            if (auto* gm = GM(OwningPlayer))
                gm->SyncLobbyBots();
        }
    }

    void ULeonTournamentLobbyWidget::OnAdjustBotsTeam2(int InDelta) {
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_UIClick", 0.4f);
        if (auto* gi = GI())
            gi->AdjustDesiredBotsTeam2(InDelta);
        RefreshBotLabels();
        if (auto* gs = GS(OwningPlayer);
            gs && gs->GetMatchState() == ELeonTournamentMatchState::Lobby) {
            if (auto* gm = GM(OwningPlayer))
                gm->SyncLobbyBots();
        }
    }

    void ULeonTournamentLobbyWidget::RefreshCharacterLabel() {
        if (!CharacterLabel)
            return;
        auto* gi = GI();
        const auto skin = gi ? gi->GetSelectedCharacterSkin() : ELeonTournamentCharacterSkin::YBot;
        CharacterLabel->SetText(LeonTournamentCharacterSkinName(skin));
    }

    void ULeonTournamentLobbyWidget::OnPrevCharacter() {
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_UIClick", 0.45f);
        if (auto* gi = GI())
            gi->CycleSelectedCharacterSkin(-1);
        RefreshCharacterLabel();
    }

    void ULeonTournamentLobbyWidget::OnNextCharacter() {
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_UIClick", 0.45f);
        if (auto* gi = GI())
            gi->CycleSelectedCharacterSkin(1);
        RefreshCharacterLabel();
    }

    void ULeonTournamentLobbyWidget::Tick(float InDeltaTime) {
        UUserWidget::Tick(InDeltaTime);
        RefreshCharacterLabel();
        if (GamepadEdge(GamepadButton::A, bPadAWasDown) || GamepadEdge(GamepadButton::Start, bPadStartWasDown))
            OnStart();
        if (GamepadEdge(GamepadButton::B, bPadBWasDown))
            OnBack();
        if (GamepadEdge(GamepadButton::LeftBumper, bPadLBWasDown))
            OnPrevCharacter();
        if (GamepadEdge(GamepadButton::RightBumper, bPadRBWasDown))
            OnNextCharacter();
        if (GamepadEdge(GamepadButton::DPadLeft, bPadDLeftWasDown))
            OnAdjustBotsTeam1(-1);
        if (GamepadEdge(GamepadButton::DPadRight, bPadDRightWasDown))
            OnAdjustBotsTeam1(1);
        if (GamepadEdge(GamepadButton::DPadDown, bPadDDownWasDown))
            OnAdjustBotsTeam2(-1);
        if (GamepadEdge(GamepadButton::DPadUp, bPadDUpWasDown))
            OnAdjustBotsTeam2(1);
        auto* gs = GS(OwningPlayer);
        if (!gs || !RosterText)
            return;
        const int cap1 = gs->GetTeam1PlayerCount();
        const int cap2 = gs->GetTeam2PlayerCount();
        std::ostringstream ss;
        ss << "TEAM 1 (" << cap1 << ")\n";
        for (APlayerState* ps : gs->GetPlayerArray()) {
            auto* sps = dynamic_cast<ALeonTournamentPlayerState*>(ps);
            if (sps && sps->GetTeam() == ELeonTournamentTeam::Team1)
                ss << "  " << sps->GetPlayerName() << (sps->IsBot() ? "  [BOT]" : "") << "  ["
                   << LeonTournamentCharacterSkinName(sps->GetCharacterSkin()) << "]\n";
        }
        ss << "\nTEAM 2 (" << cap2 << ")\n";
        for (APlayerState* ps : gs->GetPlayerArray()) {
            auto* sps = dynamic_cast<ALeonTournamentPlayerState*>(ps);
            if (sps && sps->GetTeam() == ELeonTournamentTeam::Team2)
                ss << "  " << sps->GetPlayerName() << (sps->IsBot() ? "  [BOT]" : "") << "  ["
                   << LeonTournamentCharacterSkinName(sps->GetCharacterSkin()) << "]\n";
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
        CrosshairText->SetText("");
        CrosshairText->SetFontScale(1.0f);
        CrosshairText->SetColor({0.95f, 0.97f, 1.0f, 0.0f});
        CrosshairText->SetJustification(ETextAlignment::Center);
        // Keep slot for tests/glyph API; optical center is the image dot below.
        Root->AddChild(CrosshairText, FAnchors::Center(), FMargin(-2.0f, -2.0f, -2.0f, -2.0f));

        auto centerDot = std::make_shared<UImage>("CrosshairDot");
        centerDot->SetTintColor({0.95f, 0.97f, 1.0f, 0.95f});
        Root->AddChild(centerDot, FAnchors::Center(), FMargin(-1.5f, -1.5f, -1.5f, -1.5f));

        auto makeBar = [&](const char* name) {
            auto bar = std::make_shared<UImage>(name);
            bar->SetTintColor({0.95f, 0.97f, 1.0f, 0.9f});
            Root->AddChild(bar, FAnchors::Center(), FMargin(0, 0, 0, 0));
            return bar;
        };
        CrosshairBarT = makeBar("CH_T");
        CrosshairBarB = makeBar("CH_B");
        CrosshairBarL = makeBar("CH_L");
        CrosshairBarR = makeBar("CH_R");

        for (int i = 0; i < 8; ++i) {
            auto seg = std::make_shared<UImage>(std::string("CH_Ring_") + std::to_string(i));
            seg->SetTintColor({0.95f, 0.97f, 1.0f, 0.9f});
            seg->SetVisibility(ESlateVisibility::Collapsed);
            Root->AddChild(seg, FAnchors::Center(), FMargin(0, 0, 0, 0));
            CrosshairRing[static_cast<size_t>(i)] = seg;
        }

        auto makeHit = [&](const char* name) {
            auto mark = std::make_shared<UImage>(name);
            mark->SetTintColor({1.0f, 0.85f, 0.15f, 1.0f});
            mark->SetVisibility(ESlateVisibility::Collapsed);
            Root->AddChild(mark, FAnchors::Center(), FMargin(0, 0, 0, 0));
            return mark;
        };
        HitMarkTL = makeHit("HM_TL");
        HitMarkTR = makeHit("HM_TR");
        HitMarkBL = makeHit("HM_BL");
        HitMarkBR = makeHit("HM_BR");

        HealthText = std::make_shared<UTextBlock>("HP");
        HealthText->SetFontScale(1.0f);
        HealthText->SetColor({0.4f, 0.95f, 0.5f, 1.0f});
        Root->AddChild(HealthText, FAnchors::TopLeft(), FMargin(40, 660, -300, -700));

        AmmoText = std::make_shared<UTextBlock>("Ammo");
        AmmoText->SetFontScale(1.0f);
        Root->AddChild(AmmoText, FAnchors::TopRight(), FMargin(-260, 660, 40, -700));

        WeaponSlotsText = std::make_shared<UTextBlock>("WeaponSlots");
        WeaponSlotsText->SetFontScale(0.85f);
        WeaponSlotsText->SetColor({0.8f, 0.88f, 1.0f, 0.95f});
        Root->AddChild(WeaponSlotsText, FAnchors::TopRight(), FMargin(-380, 620, 40, -655));

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
                AmmoText->SetText(std::string(LeonTournamentWeaponName(ch->GetActiveWeaponId())) + "  RELOADING");
            } else {
                AmmoText->SetText(std::string(LeonTournamentWeaponName(ch->GetActiveWeaponId())) + "  " +
                                  std::to_string(ch->GetWeapon()->GetCurrentAmmo()) + " / " +
                                  std::to_string(ch->GetWeapon()->GetMagazineSize()));
            }
        }
        if (WeaponSlotsText) {
            if (!ch || bLab) {
                WeaponSlotsText->SetVisibility(ESlateVisibility::Collapsed);
            } else {
                WeaponSlotsText->SetVisibility(ESlateVisibility::HitTestInvisible);
                std::string slots;
                const int count = static_cast<int>(ELeonTournamentWeaponId::Count);
                for (int i = 0; i < count; ++i) {
                    if (i)
                        slots += "  ";
                    const auto id = static_cast<ELeonTournamentWeaponId>(i);
                    const bool owned = ch->HasWeapon(id);
                    const bool active = ch->GetActiveWeaponId() == id;
                    if (!owned)
                        slots += ".";
                    else if (active)
                        slots += "[" + std::to_string(i + 1) + "]";
                    else
                        slots += std::to_string(i + 1);
                }
                WeaponSlotsText->SetText(slots + "   1-6 Q/E  V cam");
            }
        }
        if (HintText) {
            if (bLab) {
                HintText->SetVisibility(ESlateVisibility::HitTestInvisible);
                HintText->SetText(
                    "V = Camera   LMB = Fire   RMB = Scope   R = Reload   Alt/C = Dodge   Space x2 = Double Jump   1-6 = Weapons");
            } else {
                HintText->SetVisibility(ESlateVisibility::Collapsed);
            }
        }
        auto* spc = dynamic_cast<ALeonTournamentPlayerController*>(OwningPlayer);
        // Crosshair half-gap in HUD pixels = angular spread projected through vertical FOV.
        float fovY = 95.0f;
        if (OwningPlayer) {
            FPerspectiveCamera viewCam;
            OwningPlayer->GetPlayerViewPoint(viewCam);
            fovY = std::max(10.0f, viewCam.GetFOV());
        }
        const float hudHalfH = Root ? Root->GetSize().y * 0.5f : 360.0f;
        float spreadDeg = (ch && ch->GetWeapon()) ? ch->GetWeapon()->GetCurrentSpreadDeg() : 0.35f;
        if (ch && ch->GetWeapon())
            spreadDeg += ch->GetWeapon()->GetConfig().PelletSpreadDeg * 0.5f;
        const float tanHalfFov = std::tan(glm::radians(fovY * 0.5f));
        float gap = 4.0f;
        if (tanHalfFov > 1e-4f)
            gap = std::tan(glm::radians(std::max(0.0f, spreadDeg))) / tanHalfFov * hudHalfH;
        gap = std::clamp(gap, 2.0f, hudHalfH * 0.45f);
        constexpr float barLen = 12.0f;
        constexpr float barThick = 2.0f;
        const auto center = FAnchors::Center();
        const bool bCircle =
            ch && ch->GetWeapon() &&
            ch->GetWeapon()->GetConfig().CrosshairStyle == ELeonTournamentCrosshairStyle::Circle;
        if (Root) {
            if (CrosshairBarT)
                Root->SetChildLayout(CrosshairBarT, center,
                                     FMargin(-(barLen * 0.5f), -(gap + barThick), -(barLen * 0.5f), gap));
            if (CrosshairBarB)
                Root->SetChildLayout(CrosshairBarB, center,
                                     FMargin(-(barLen * 0.5f), gap, -(barLen * 0.5f), -(gap + barThick)));
            if (CrosshairBarL)
                Root->SetChildLayout(CrosshairBarL, center,
                                     FMargin(-(gap + barThick), -(barLen * 0.5f), gap, -(barLen * 0.5f)));
            if (CrosshairBarR)
                Root->SetChildLayout(CrosshairBarR, center,
                                     FMargin(gap, -(barLen * 0.5f), -(gap + barThick), -(barLen * 0.5f)));
            constexpr float segHalf = 3.5f;
            constexpr float segThick = 2.0f;
            for (int i = 0; i < 8; ++i) {
                if (!CrosshairRing[static_cast<size_t>(i)])
                    continue;
                const float ang = static_cast<float>(i) * 0.78539816f;
                const float cx = std::cos(ang) * gap;
                const float cy = std::sin(ang) * gap;
                Root->SetChildLayout(CrosshairRing[static_cast<size_t>(i)], center,
                                     FMargin(cx - segHalf, cy - segThick, -(cx + segHalf), -(cy + segThick)));
            }
        }
        const bool bHit = spc && spc->IsHitMarkerActive();
        constexpr float hitLen = 9.0f;
        constexpr float hitThick = 2.5f;
        if (Root && HitMarkTL && HitMarkTR && HitMarkBL && HitMarkBR) {
            const float d = gap + 4.0f;
            Root->SetChildLayout(HitMarkTL, center,
                                 FMargin(-(d + hitLen), -(d + hitThick), d, d - hitThick));
            Root->SetChildLayout(HitMarkTR, center,
                                 FMargin(d, -(d + hitThick), -(d + hitLen), d - hitThick));
            Root->SetChildLayout(HitMarkBL, center,
                                 FMargin(-(d + hitLen), d - hitThick, d, -(d + hitThick)));
            Root->SetChildLayout(HitMarkBR, center,
                                 FMargin(d, d - hitThick, -(d + hitLen), -(d + hitThick)));
        }
        const auto barVis = (!bCircle) ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
        const auto ringVis = bCircle ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
        const auto hitVis = bHit ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
        if (CrosshairBarT)
            CrosshairBarT->SetVisibility(barVis);
        if (CrosshairBarB)
            CrosshairBarB->SetVisibility(barVis);
        if (CrosshairBarL)
            CrosshairBarL->SetVisibility(barVis);
        if (CrosshairBarR)
            CrosshairBarR->SetVisibility(barVis);
        for (auto& seg : CrosshairRing) {
            if (seg)
                seg->SetVisibility(ringVis);
        }
        if (HitMarkTL)
            HitMarkTL->SetVisibility(hitVis);
        if (HitMarkTR)
            HitMarkTR->SetVisibility(hitVis);
        if (HitMarkBL)
            HitMarkBL->SetVisibility(hitVis);
        if (HitMarkBR)
            HitMarkBR->SetVisibility(hitVis);
        if (CrosshairText) {
            CrosshairText->SetVisibility(ESlateVisibility::HitTestInvisible);
            CrosshairText->SetText(bHit ? "X" : "");
            CrosshairText->SetFontScale(bHit ? 1.35f : 1.0f);
            CrosshairText->SetColor(bHit ? glm::vec4(1.0f, 0.85f, 0.15f, 1.0f) : glm::vec4(0.95f, 0.97f, 1.0f, 0.0f));
            if (Root) {
                if (bHit)
                    Root->SetChildLayout(CrosshairText, center, FMargin(-10.0f, -12.0f, -10.0f, -8.0f));
                else
                    Root->SetChildLayout(CrosshairText, center, FMargin(-2.0f, -2.0f, -2.0f, -2.0f));
            }
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
        if (GamepadEdge(GamepadButton::A, bPadAWasDown) || GamepadEdge(GamepadButton::Start, bPadStartWasDown))
            OnReturn();
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
