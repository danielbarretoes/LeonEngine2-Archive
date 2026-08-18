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
        Root->SetBackgroundColor({0.03f, 0.04f, 0.08f, 0.90f});

        constexpr float leftX = 56.0f;
        float y = 28.0f;

        TitleText = std::make_shared<UTextBlock>("LobbyTitle");
        TitleText->SetText("LOBBY  â€”  UP TO 12");
        TitleText->SetFontScale(kFsTitle);
        PlaceTextTL(*Root, TitleText, leftX, y);
        y += MeasurePadded(TitleText->GetText(), TitleText->GetFontScale()).y + 12.0f;

        RosterText = std::make_shared<UTextBlock>("Roster");
        RosterText->SetFontScale(kFsBody);
        RosterText->SetText("TEAM 1 (0)\n\nTEAM 2 (0)");
        const float rosterH = MeasurePadded(std::string(14, '\n') + "x", kFsBody).y;
        RosterScroll = std::make_shared<UScrollBox>("RosterScroll");
        RosterScroll->SetSize({1160.0f, rosterH});
        RosterScroll->SetContentHeight(rosterH);
        RosterText->SetPosition({8.0f, 8.0f});
        RosterText->SetSize({1140.0f, rosterH});
        RosterScroll->AddChild(RosterText);
        Root->AddChild(RosterScroll, FAnchors::TopLeft(), BoxTL(leftX, y, 1160.0f, rosterH));
        y += rosterH + 16.0f;

        auto botsTitle = std::make_shared<UTextBlock>("BotsTitle");
        botsTitle->SetText("BOTS PER TEAM");
        botsTitle->SetFontScale(kFsCaption);
        botsTitle->SetColor({0.65f, 0.72f, 0.85f, 1.0f});
        PlaceTextTL(*Root, botsTitle, leftX, y);
        y += MeasurePadded(botsTitle->GetText(), botsTitle->GetFontScale()).y + 8.0f;

        auto t1Minus = MakeButton("T1BotsMinus", "-", kFsLabel, 52.0f, 48.0f);
        t1Minus->OnClicked.AddLambda([this]() { OnAdjustBotsTeam1(-1); });
        PlaceButtonTL(*Root, t1Minus, leftX, y);

        BotsTeam1Label = std::make_shared<UTextBlock>("T1BotsLabel");
        BotsTeam1Label->SetFontScale(kFsLabel);
        BotsTeam1Label->SetJustification(ETextAlignment::Center);
        BotsTeam1Label->SetText("TEAM 1 BOTS: 0");
        const float botsLabelW = 260.0f;
        const float botsLabelH = MeasurePadded("TEAM 1 BOTS: 99", kFsLabel).y;
        Root->AddChild(BotsTeam1Label, FAnchors::TopLeft(),
                       BoxTL(leftX + t1Minus->GetSize().x + 10.0f, y + (t1Minus->GetSize().y - botsLabelH) * 0.5f,
                             botsLabelW, botsLabelH));

        auto t1Plus = MakeButton("T1BotsPlus", "+", kFsLabel, 52.0f, 48.0f);
        t1Plus->OnClicked.AddLambda([this]() { OnAdjustBotsTeam1(1); });
        PlaceButtonTL(*Root, t1Plus, leftX + t1Minus->GetSize().x + 10.0f + botsLabelW + 10.0f, y);

        BotsTeam1Slider = std::make_shared<USlider>("T1BotsSlider");
        BotsTeam1Slider->SetSize({220.0f, 22.0f});
        BotsTeam1Slider->OnValueChanged = [this](float InValue) {
            if (auto* gi = GI()) {
                const int count = static_cast<int>(std::round(InValue * 6.0f));
                if (count != gi->GetDesiredBotsTeam1()) {
                    gi->SetDesiredBotsTeam1(count);
                    RefreshBotLabels();
                    if (auto* gs = GS(OwningPlayer); gs && gs->GetMatchState() == ELeonTournamentMatchState::Lobby) {
                        if (auto* gm = GM(OwningPlayer))
                            gm->SyncLobbyBots();
                    }
                }
            }
        };
        Root->AddChild(BotsTeam1Slider, FAnchors::TopLeft(), BoxTL(leftX + 430.0f, y + 12.0f, 220.0f, 22.0f));
        y += t1Minus->GetSize().y + 10.0f;

        auto t2Minus = MakeButton("T2BotsMinus", "-", kFsLabel, 52.0f, 48.0f);
        t2Minus->OnClicked.AddLambda([this]() { OnAdjustBotsTeam2(-1); });
        PlaceButtonTL(*Root, t2Minus, leftX, y);

        BotsTeam2Label = std::make_shared<UTextBlock>("T2BotsLabel");
        BotsTeam2Label->SetFontScale(kFsLabel);
        BotsTeam2Label->SetJustification(ETextAlignment::Center);
        BotsTeam2Label->SetText("TEAM 2 BOTS: 0");
        Root->AddChild(BotsTeam2Label, FAnchors::TopLeft(),
                       BoxTL(leftX + t2Minus->GetSize().x + 10.0f, y + (t2Minus->GetSize().y - botsLabelH) * 0.5f,
                             botsLabelW, botsLabelH));

        auto t2Plus = MakeButton("T2BotsPlus", "+", kFsLabel, 52.0f, 48.0f);
        t2Plus->OnClicked.AddLambda([this]() { OnAdjustBotsTeam2(1); });
        PlaceButtonTL(*Root, t2Plus, leftX + t2Minus->GetSize().x + 10.0f + botsLabelW + 10.0f, y);
        y += t2Minus->GetSize().y + 10.0f;

        CapacityHint = std::make_shared<UTextBlock>("CapacityHint");
        CapacityHint->SetFontScale(kFsCaption);
        CapacityHint->SetColor({0.7f, 0.78f, 0.9f, 1.0f});
        CapacityHint->SetText("Capacity ~1 / 12  (you + bots)   D-Pad adjust");
        PlaceTextTL(*Root, CapacityHint, leftX, y, 700.0f);
        y += MeasurePadded(CapacityHint->GetText(), CapacityHint->GetFontScale()).y + 16.0f;

        constexpr float charX = 620.0f;
        float charY = 430.0f;

        auto charTitle = std::make_shared<UTextBlock>("LobbyCharTitle");
        charTitle->SetText("YOUR CHARACTER");
        charTitle->SetFontScale(kFsCaption);
        charTitle->SetColor({0.65f, 0.72f, 0.85f, 1.0f});
        PlaceTextTL(*Root, charTitle, charX, charY);
        charY += MeasurePadded(charTitle->GetText(), charTitle->GetFontScale()).y + 8.0f;

        auto prevChar = MakeButton("LobbyPrevChar", "<", kFsSub, 56.0f, 52.0f);
        prevChar->OnClicked.AddLambda([this]() { OnPrevCharacter(); });
        PlaceButtonTL(*Root, prevChar, charX, charY);

        CharacterLabel = std::make_shared<UTextBlock>("LobbyCharName");
        CharacterLabel->SetFontScale(kFsSub);
        CharacterLabel->SetColor({0.95f, 0.97f, 1.0f, 1.0f});
        CharacterLabel->SetJustification(ETextAlignment::Center);
        CharacterLabel->SetText("PATRICK");
        const float nameW = 220.0f;
        const float nameH = MeasurePadded("PATRICK", kFsSub).y;
        Root->AddChild(
            CharacterLabel, FAnchors::TopLeft(),
            BoxTL(charX + prevChar->GetSize().x + 12.0f, charY + (prevChar->GetSize().y - nameH) * 0.5f, nameW, nameH));
        RefreshCharacterLabel();

        auto nextChar = MakeButton("LobbyNextChar", ">", kFsSub, 56.0f, 52.0f);
        nextChar->OnClicked.AddLambda([this]() { OnNextCharacter(); });
        PlaceButtonTL(*Root, nextChar, charX + prevChar->GetSize().x + 12.0f + nameW + 12.0f, charY);
        charY += prevChar->GetSize().y + 14.0f;

        auto mapTitle = std::make_shared<UTextBlock>("LobbyMapTitle");
        mapTitle->SetText("MAP");
        mapTitle->SetFontScale(kFsCaption);
        mapTitle->SetColor({0.65f, 0.72f, 0.85f, 1.0f});
        PlaceTextTL(*Root, mapTitle, charX, charY);
        charY += MeasurePadded(mapTitle->GetText(), mapTitle->GetFontScale()).y + 8.0f;

        auto prevMap = MakeButton("LobbyPrevMap", "<", kFsSub, 56.0f, 52.0f);
        prevMap->OnClicked.AddLambda([this]() { OnPrevMap(); });
        PlaceButtonTL(*Root, prevMap, charX, charY);

        MapLabel = std::make_shared<UTextBlock>("LobbyMapName");
        MapLabel->SetFontScale(kFsSub);
        MapLabel->SetColor({0.95f, 0.97f, 1.0f, 1.0f});
        MapLabel->SetJustification(ETextAlignment::Center);
        MapLabel->SetText("ARENA");
        Root->AddChild(
            MapLabel, FAnchors::TopLeft(),
            BoxTL(charX + prevMap->GetSize().x + 12.0f, charY + (prevMap->GetSize().y - nameH) * 0.5f, nameW, nameH));
        RefreshMapLabel();

        auto nextMap = MakeButton("LobbyNextMap", ">", kFsSub, 56.0f, 52.0f);
        nextMap->OnClicked.AddLambda([this]() { OnNextMap(); });
        PlaceButtonTL(*Root, nextMap, charX + prevMap->GetSize().x + 12.0f + nameW + 12.0f, charY);

        auto start = MakeButton("Start", "START MATCH", kFsButton, 240.0f);
        start->OnClicked.AddLambda([this]() { OnStart(); });
        PlaceButtonTL(*Root, start, leftX, std::max(y, 600.0f));

        auto back = MakeButton("Back", "BACK", kFsButton, 160.0f);
        back->OnClicked.AddLambda([this]() { OnBack(); });
        PlaceButtonTL(*Root, back, leftX + start->GetSize().x + 16.0f, std::max(y, 600.0f));

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
        if (BotsTeam1Slider)
            BotsTeam1Slider->SetValue(static_cast<float>(t1) / 6.0f);
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
        // Only spawn bots while the lobby is active â€” never during MainMenu widget construct.
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

    void ULeonTournamentLobbyWidget::Tick(float InDeltaTime) {
        UUserWidget::Tick(InDeltaTime);
        RefreshCharacterLabel();
        RefreshMapLabel();
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
