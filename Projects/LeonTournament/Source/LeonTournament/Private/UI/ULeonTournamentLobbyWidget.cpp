#include "ULeonTournamentWidgets.hpp"
#include "FLeonTournamentUILayout.hpp"
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
#include "UMG/USlider.hpp"
#include "UMG/UCheckBox.hpp"
#include "UMG/UWidgetSwitcher.hpp"
#include "UMG/UScrollBox.hpp"
#include "Renderer/FPerspectiveCamera.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <glm/glm.hpp>

namespace Leon {

    namespace {
        ULeonTournamentGameInstance* GI() {
            return FLeonTournamentUILayout::GI();
        }
        bool GamepadEdge(int InButton, bool& InOutWasDown) {
            return FLeonTournamentUILayout::GamepadEdge(InButton, InOutWasDown);
        }
        ALeonTournamentGameMode* GM(APlayerController* InPC) {
            return FLeonTournamentUILayout::GM(InPC);
        }
        ALeonTournamentGameState* GS(APlayerController* InPC) {
            return FLeonTournamentUILayout::GS(InPC);
        }
        bool IsClientWorld(APlayerController* InPC) {
            return FLeonTournamentUILayout::IsClientWorld(InPC);
        }
        constexpr float kFsCaption = FLeonTournamentUILayout::kFsCaption;
        constexpr float kFsBody = FLeonTournamentUILayout::kFsBody;
        constexpr float kFsLabel = FLeonTournamentUILayout::kFsLabel;
        constexpr float kFsButton = FLeonTournamentUILayout::kFsButton;
        constexpr float kFsSub = FLeonTournamentUILayout::kFsSub;
        constexpr float kFsTitle = FLeonTournamentUILayout::kFsTitle;
        constexpr float kFsHero = FLeonTournamentUILayout::kFsHero;
        constexpr float kFsScore = FLeonTournamentUILayout::kFsScore;
        constexpr float kFsTimer = FLeonTournamentUILayout::kFsTimer;
        constexpr float kFsVital = FLeonTournamentUILayout::kFsVital;
        constexpr float kFsBanner = FLeonTournamentUILayout::kFsBanner;

        FMargin BoxTL(float InX, float InY, float InW, float InH) {
            return FLeonTournamentUILayout::BoxTL(InX, InY, InW, InH);
        }
        FMargin BoxBL(float InX, float InBottom, float InW, float InH) {
            return FLeonTournamentUILayout::BoxBL(InX, InBottom, InW, InH);
        }
        FMargin BoxBR(float InRight, float InBottom, float InW, float InH) {
            return FLeonTournamentUILayout::BoxBR(InRight, InBottom, InW, InH);
        }
        FMargin BoxTC(float InTop, float InW, float InH, float InOx = 0.0f) {
            return FLeonTournamentUILayout::BoxTC(InTop, InW, InH, InOx);
        }
        FMargin BoxBC(float InBottom, float InW, float InH, float InOx = 0.0f) {
            return FLeonTournamentUILayout::BoxBC(InBottom, InW, InH, InOx);
        }
        FMargin BoxC(float InOx, float InOy, float InW, float InH) {
            return FLeonTournamentUILayout::BoxC(InOx, InOy, InW, InH);
        }
        glm::vec2 MeasurePadded(const std::string& InText, float InScale, float InPadX = 8.0f, float InPadY = 6.0f) {
            return FLeonTournamentUILayout::MeasurePadded(InText, InScale, InPadX, InPadY);
        }
        TRef<UButton> MakeButton(const std::string& InName, const std::string& InLabel, float InFont = kFsButton,
                                 float InMinW = 200.0f, float InMinH = 0.0f) {
            return FLeonTournamentUILayout::MakeButton(InName, InLabel, InFont, InMinW, InMinH);
        }
        void PlaceButtonTL(UCanvasPanel& InRoot, const TRef<UButton>& InBtn, float InX, float InY) {
            FLeonTournamentUILayout::PlaceButtonTL(InRoot, InBtn, InX, InY);
        }
        void PlaceButtonC(UCanvasPanel& InRoot, const TRef<UButton>& InBtn, float InOx, float InOy) {
            FLeonTournamentUILayout::PlaceButtonC(InRoot, InBtn, InOx, InOy);
        }
        void PlaceTextTL(UCanvasPanel& InRoot, const TRef<UTextBlock>& InText, float InX, float InY,
                         float InMinW = 0.0f, float InMinH = 0.0f) {
            FLeonTournamentUILayout::PlaceTextTL(InRoot, InText, InX, InY, InMinW, InMinH);
        }
        void PlaceTextTC(UCanvasPanel& InRoot, const TRef<UTextBlock>& InText, float InTop, float InMinW = 0.0f,
                         float InOx = 0.0f) {
            FLeonTournamentUILayout::PlaceTextTC(InRoot, InText, InTop, InMinW, InOx);
        }
        void PlaceTextBL(UCanvasPanel& InRoot, const TRef<UTextBlock>& InText, float InX, float InBottom,
                         float InMinW = 0.0f) {
            FLeonTournamentUILayout::PlaceTextBL(InRoot, InText, InX, InBottom, InMinW);
        }
        void PlaceTextBR(UCanvasPanel& InRoot, const TRef<UTextBlock>& InText, float InRight, float InBottom,
                         float InMinW = 0.0f) {
            FLeonTournamentUILayout::PlaceTextBR(InRoot, InText, InRight, InBottom, InMinW);
        }
        void PlaceTextBC(UCanvasPanel& InRoot, const TRef<UTextBlock>& InText, float InBottom, float InMinW = 0.0f) {
            FLeonTournamentUILayout::PlaceTextBC(InRoot, InText, InBottom, InMinW);
        }
        void PlaceTextC(UCanvasPanel& InRoot, const TRef<UTextBlock>& InText, float InOx, float InOy,
                        float InMinW = 0.0f, float InMinH = 0.0f) {
            FLeonTournamentUILayout::PlaceTextC(InRoot, InText, InOx, InOy, InMinW, InMinH);
        }
    } // namespace

    ULeonTournamentLobbyWidget::ULeonTournamentLobbyWidget(const std::string& InName) : UUserWidget(InName) {}
    void ULeonTournamentLobbyWidget::Construct() {
        Build();
    }

    void ULeonTournamentLobbyWidget::Build() {
        FUIRenderer::Init();
        Root = std::make_shared<UCanvasPanel>("LobbyRoot");
        Root->SetSize({1280, 720});
        Root->SetBackgroundColor({0.0f, 0.0f, 0.0f, 0.0f});

        Panel = std::make_shared<UImage>("LobbyPanel");
        Panel->SetTintColor({0.03f, 0.04f, 0.08f, 0.92f});
        Root->AddChild(Panel, FAnchors::LeftStretch(),
                       FUILayout::BoxLeftStretch(0.0f, kLeonTournamentLobbyPanelDesignWidth));

        constexpr float leftX = 36.0f;
        float y = 28.0f;

        TitleText = std::make_shared<UTextBlock>("LobbyTitle");
        TitleText->SetText("LOBBY");
        TitleText->SetFontScale(kFsTitle);
        PlaceTextTL(*Root, TitleText, leftX, y);
        y += MeasurePadded(TitleText->GetText(), TitleText->GetFontScale()).y + 10.0f;

        RosterText = std::make_shared<UTextBlock>("Roster");
        RosterText->SetFontScale(kFsBody);
        RosterText->SetText("TEAM 1 (0)\n\nTEAM 2 (0)");
        const float rosterH = MeasurePadded(std::string(10, '\n') + "x", kFsBody).y;
        RosterScroll = std::make_shared<UScrollBox>("RosterScroll");
        RosterScroll->SetSize({448.0f, rosterH});
        RosterScroll->SetContentHeight(rosterH);
        RosterText->SetPosition({8.0f, 8.0f});
        RosterText->SetSize({428.0f, rosterH});
        RosterScroll->AddChild(RosterText);
        Root->AddChild(RosterScroll, FAnchors::TopLeft(), BoxTL(leftX, y, 448.0f, rosterH));
        y += rosterH + 12.0f;

        auto botsTitle = std::make_shared<UTextBlock>("BotsTitle");
        botsTitle->SetText("BOTS");
        botsTitle->SetFontScale(kFsCaption);
        botsTitle->SetColor({0.65f, 0.72f, 0.85f, 1.0f});
        PlaceTextTL(*Root, botsTitle, leftX, y);
        y += MeasurePadded(botsTitle->GetText(), botsTitle->GetFontScale()).y + 6.0f;

        auto t1Minus = MakeButton("T1BotsMinus", "-", kFsLabel, 48.0f, 44.0f);
        t1Minus->OnClicked.AddLambda([this]() { OnAdjustBotsTeam1(-1); });
        PlaceButtonTL(*Root, t1Minus, leftX, y);

        BotsTeam1Label = std::make_shared<UTextBlock>("T1BotsLabel");
        BotsTeam1Label->SetFontScale(kFsLabel);
        BotsTeam1Label->SetJustification(ETextAlignment::Center);
        BotsTeam1Label->SetText("TEAM 1: 0");
        const float botsLabelW = 220.0f;
        const float botsLabelH = MeasurePadded("TEAM 1: 99", kFsLabel).y;
        Root->AddChild(BotsTeam1Label, FAnchors::TopLeft(),
                       BoxTL(leftX + t1Minus->GetSize().x + 8.0f, y + (t1Minus->GetSize().y - botsLabelH) * 0.5f,
                             botsLabelW, botsLabelH));

        auto t1Plus = MakeButton("T1BotsPlus", "+", kFsLabel, 48.0f, 44.0f);
        t1Plus->OnClicked.AddLambda([this]() { OnAdjustBotsTeam1(1); });
        PlaceButtonTL(*Root, t1Plus, leftX + t1Minus->GetSize().x + 8.0f + botsLabelW + 8.0f, y);
        y += t1Minus->GetSize().y + 8.0f;

        auto t2Minus = MakeButton("T2BotsMinus", "-", kFsLabel, 48.0f, 44.0f);
        t2Minus->OnClicked.AddLambda([this]() { OnAdjustBotsTeam2(-1); });
        PlaceButtonTL(*Root, t2Minus, leftX, y);

        BotsTeam2Label = std::make_shared<UTextBlock>("T2BotsLabel");
        BotsTeam2Label->SetFontScale(kFsLabel);
        BotsTeam2Label->SetJustification(ETextAlignment::Center);
        BotsTeam2Label->SetText("TEAM 2: 0");
        Root->AddChild(BotsTeam2Label, FAnchors::TopLeft(),
                       BoxTL(leftX + t2Minus->GetSize().x + 8.0f, y + (t2Minus->GetSize().y - botsLabelH) * 0.5f,
                             botsLabelW, botsLabelH));

        auto t2Plus = MakeButton("T2BotsPlus", "+", kFsLabel, 48.0f, 44.0f);
        t2Plus->OnClicked.AddLambda([this]() { OnAdjustBotsTeam2(1); });
        PlaceButtonTL(*Root, t2Plus, leftX + t2Minus->GetSize().x + 8.0f + botsLabelW + 8.0f, y);
        y += t2Minus->GetSize().y + 8.0f;

        CapacityHint = std::make_shared<UTextBlock>("CapacityHint");
        CapacityHint->SetFontScale(kFsCaption);
        CapacityHint->SetColor({0.7f, 0.78f, 0.9f, 1.0f});
        CapacityHint->SetText("You + bots  /  12");
        PlaceTextTL(*Root, CapacityHint, leftX, y, 448.0f);
        y += MeasurePadded(CapacityHint->GetText(), CapacityHint->GetFontScale()).y + 14.0f;

        auto mapTitle = std::make_shared<UTextBlock>("LobbyMapTitle");
        mapTitle->SetText("MAP");
        mapTitle->SetFontScale(kFsCaption);
        mapTitle->SetColor({0.65f, 0.72f, 0.85f, 1.0f});
        PlaceTextTL(*Root, mapTitle, leftX, y);
        y += MeasurePadded(mapTitle->GetText(), mapTitle->GetFontScale()).y + 6.0f;

        auto prevMap = MakeButton("LobbyPrevMap", "<", kFsSub, 48.0f, 44.0f);
        prevMap->OnClicked.AddLambda([this]() { OnPrevMap(); });
        PlaceButtonTL(*Root, prevMap, leftX, y);

        MapLabel = std::make_shared<UTextBlock>("LobbyMapName");
        MapLabel->SetFontScale(kFsSub);
        MapLabel->SetColor({0.95f, 0.97f, 1.0f, 1.0f});
        MapLabel->SetJustification(ETextAlignment::Center);
        MapLabel->SetText("ARENA");
        const float cycleLabelW = 240.0f;
        const float nameH = MeasurePadded("ORBITAL PRISM", kFsSub).y;
        Root->AddChild(
            MapLabel, FAnchors::TopLeft(),
            BoxTL(leftX + prevMap->GetSize().x + 8.0f, y + (prevMap->GetSize().y - nameH) * 0.5f, cycleLabelW, nameH));
        RefreshMapLabel();

        auto nextMap = MakeButton("LobbyNextMap", ">", kFsSub, 48.0f, 44.0f);
        nextMap->OnClicked.AddLambda([this]() { OnNextMap(); });
        PlaceButtonTL(*Root, nextMap, leftX + prevMap->GetSize().x + 8.0f + cycleLabelW + 8.0f, y);
        y += prevMap->GetSize().y + 10.0f;

        auto modeTitle = std::make_shared<UTextBlock>("LobbyModeTitle");
        modeTitle->SetText("MODE");
        modeTitle->SetFontScale(kFsCaption);
        modeTitle->SetColor({0.65f, 0.72f, 0.85f, 1.0f});
        PlaceTextTL(*Root, modeTitle, leftX, y);
        y += MeasurePadded(modeTitle->GetText(), modeTitle->GetFontScale()).y + 6.0f;

        auto prevMode = MakeButton("LobbyPrevMode", "<", kFsSub, 48.0f, 44.0f);
        prevMode->OnClicked.AddLambda([this]() { OnPrevGameMode(); });
        PlaceButtonTL(*Root, prevMode, leftX, y);

        GameModeLabel = std::make_shared<UTextBlock>("LobbyModeName");
        GameModeLabel->SetFontScale(kFsSub);
        GameModeLabel->SetColor({0.95f, 0.97f, 1.0f, 1.0f});
        GameModeLabel->SetJustification(ETextAlignment::Center);
        GameModeLabel->SetText("TDM");
        Root->AddChild(GameModeLabel, FAnchors::TopLeft(),
                       BoxTL(leftX + prevMode->GetSize().x + 8.0f, y + (prevMode->GetSize().y - nameH) * 0.5f,
                             cycleLabelW, nameH));
        RefreshGameModeLabel();

        auto nextMode = MakeButton("LobbyNextMode", ">", kFsSub, 48.0f, 44.0f);
        nextMode->OnClicked.AddLambda([this]() { OnNextGameMode(); });
        PlaceButtonTL(*Root, nextMode, leftX + prevMode->GetSize().x + 8.0f + cycleLabelW + 8.0f, y);
        y += prevMode->GetSize().y + 16.0f;

        auto start = MakeButton("Start", "START MATCH", kFsButton, 220.0f);
        start->OnClicked.AddLambda([this]() { OnStart(); });
        PlaceButtonTL(*Root, start, leftX, y);

        auto back = MakeButton("Back", "BACK", kFsButton, 140.0f);
        back->OnClicked.AddLambda([this]() { OnBack(); });
        PlaceButtonTL(*Root, back, leftX + start->GetSize().x + 12.0f, y);

        PrevCharBtn = MakeButton("LobbyPrevChar", "<", kFsSub, 56.0f, 52.0f);
        PrevCharBtn->OnClicked.AddLambda([this]() { OnPrevCharacter(); });

        CharacterLabel = std::make_shared<UTextBlock>("LobbyCharName");
        CharacterLabel->SetFontScale(kFsSub);
        CharacterLabel->SetColor({0.95f, 0.97f, 1.0f, 1.0f});
        CharacterLabel->SetJustification(ETextAlignment::Center);
        CharacterLabel->SetText("YBOT");
        RefreshCharacterLabel();

        NextCharBtn = MakeButton("LobbyNextChar", ">", kFsSub, 56.0f, 52.0f);
        NextCharBtn->OnClicked.AddLambda([this]() { OnNextCharacter(); });

        RefreshBotLabels();
        SetWidgetTree(Root);
        ApplyViewportLayout();
    }

    void ULeonTournamentLobbyWidget::ApplyViewportLayout() {
        if (!Root)
            return;
        const glm::vec2 vp = FLeonTournamentUILayout::ResolveViewportSize(Root.get());
        AppliedViewport = vp;
        SetSize(vp);
        FLeonTournamentUILayout::ApplyMenuRailLayout(*Root, Panel, PrevCharBtn, CharacterLabel, NextCharBtn, true);
    }

    void ULeonTournamentLobbyWidget::RefreshBotLabels() {
        auto* gi = GI();
        const int t1 = gi ? gi->GetDesiredBotsTeam1() : 0;
        const int t2 = gi ? gi->GetDesiredBotsTeam2() : 0;
        if (BotsTeam1Label)
            BotsTeam1Label->SetText("TEAM 1: " + std::to_string(t1));
        if (BotsTeam2Label)
            BotsTeam2Label->SetText("TEAM 2: " + std::to_string(t2));
        if (CapacityHint) {
            const int total = 1 + t1 + t2;
            CapacityHint->SetText("You + bots  " + std::to_string(total) + "  /  12");
        }
    }

    void ULeonTournamentLobbyWidget::OnAdjustBotsTeam1(int InDelta) {
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_UIClick", 0.4f);
        if (auto* gi = GI())
            gi->AdjustDesiredBotsTeam1(InDelta);
        RefreshBotLabels();
        // Only spawn bots while the lobby is active — never during MainMenu widget construct.
        if (auto* gs = GS(OwningPlayer); gs && gs->GetMatchState() == ELeonTournamentMatchState::Lobby) {
            if (auto* gm = GM(OwningPlayer))
                gm->SyncLobbyBots();
        }
    }

    void ULeonTournamentLobbyWidget::OnAdjustBotsTeam2(int InDelta) {
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_UIClick", 0.4f);
        if (auto* gi = GI())
            gi->AdjustDesiredBotsTeam2(InDelta);
        RefreshBotLabels();
        if (auto* gs = GS(OwningPlayer); gs && gs->GetMatchState() == ELeonTournamentMatchState::Lobby) {
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

    void ULeonTournamentLobbyWidget::NotifyCharacterCycled() {
        RefreshCharacterLabel();
        if (auto* gm = GM(OwningPlayer))
            gm->NotifySelectedCharacterChanged();
    }

    void ULeonTournamentLobbyWidget::OnPrevCharacter() {
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_UIClick", 0.45f);
        if (auto* gi = GI())
            gi->CycleSelectedCharacterSkin(-1);
        NotifyCharacterCycled();
    }

    void ULeonTournamentLobbyWidget::OnNextCharacter() {
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_UIClick", 0.45f);
        if (auto* gi = GI())
            gi->CycleSelectedCharacterSkin(1);
        NotifyCharacterCycled();
    }

    void ULeonTournamentLobbyWidget::RefreshMapLabel() {
        if (!MapLabel)
            return;
        auto* gi = GI();
        const auto map = gi ? gi->GetSelectedPlayableMap() : ELeonTournamentPlayableMap::Arena;
        MapLabel->SetText(LeonTournamentPlayableMapName(map));
    }

    void ULeonTournamentLobbyWidget::OnPrevMap() {
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_UIClick", 0.45f);
        if (auto* gi = GI())
            gi->CycleSelectedPlayableMap(-1);
        RefreshMapLabel();
    }

    void ULeonTournamentLobbyWidget::OnNextMap() {
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_UIClick", 0.45f);
        if (auto* gi = GI())
            gi->CycleSelectedPlayableMap(1);
        RefreshMapLabel();
    }

    void ULeonTournamentLobbyWidget::RefreshGameModeLabel() {
        if (!GameModeLabel)
            return;
        auto* gi = GI();
        const auto mode = gi ? gi->GetSelectedGameMode() : ELeonTournamentGameModeId::TeamDeathmatch;
        GameModeLabel->SetText(LeonTournamentGameModeName(mode));
    }

    void ULeonTournamentLobbyWidget::OnPrevGameMode() {
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_UIClick", 0.45f);
        if (auto* gi = GI())
            gi->CycleSelectedGameMode(-1);
        RefreshGameModeLabel();
    }

    void ULeonTournamentLobbyWidget::OnNextGameMode() {
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_UIClick", 0.45f);
        if (auto* gi = GI())
            gi->CycleSelectedGameMode(1);
        RefreshGameModeLabel();
    }

    void ULeonTournamentLobbyWidget::Tick(float InDeltaTime) {
        UUserWidget::Tick(InDeltaTime);
        const glm::vec2 vp = FLeonTournamentUILayout::ResolveViewportSize(Root.get());
        if (glm::length(vp - AppliedViewport) > 1.0f)
            ApplyViewportLayout();
        RefreshCharacterLabel();
        RefreshMapLabel();
        RefreshGameModeLabel();
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
        if (auto* gm = GM(OwningPlayer)) {
            auto* gi = GI();
            const auto map = gi ? gi->GetSelectedPlayableMap() : ELeonTournamentPlayableMap::Arena;
            if (map == ELeonTournamentPlayableMap::Arena)
                gm->RequestStartMatch();
            else
                gm->OpenPlayableMap(map);
        }
    }

    void ULeonTournamentLobbyWidget::OnBack() {
        if (IsClientWorld(OwningPlayer))
            return;
        if (auto* gm = GM(OwningPlayer))
            gm->ReturnToMenu();
    }

} // namespace Leon
