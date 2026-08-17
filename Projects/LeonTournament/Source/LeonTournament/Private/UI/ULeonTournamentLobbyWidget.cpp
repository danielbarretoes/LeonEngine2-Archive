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
#include "Renderer/FPerspectiveCamera.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <glm/glm.hpp>

namespace Leon {

    namespace {
        using Layout = FLeonTournamentUILayout;
        ULeonTournamentGameInstance* GI() { return Layout::GI(); }
        bool GamepadEdge(int InButton, bool& InOutWasDown) { return Layout::GamepadEdge(InButton, InOutWasDown); }
        ALeonTournamentGameMode* GM(APlayerController* InPC) { return Layout::GM(InPC); }
        ALeonTournamentGameState* GS(APlayerController* InPC) { return Layout::GS(InPC); }
        bool IsClientWorld(APlayerController* InPC) { return Layout::IsClientWorld(InPC); }
        constexpr float kFsCaption = Layout::kFsCaption;
        constexpr float kFsBody = Layout::kFsBody;
        constexpr float kFsLabel = Layout::kFsLabel;
        constexpr float kFsButton = Layout::kFsButton;
        constexpr float kFsSub = Layout::kFsSub;
        constexpr float kFsTitle = Layout::kFsTitle;
        constexpr float kFsHero = Layout::kFsHero;
        constexpr float kFsScore = Layout::kFsScore;
        constexpr float kFsTimer = Layout::kFsTimer;
        constexpr float kFsVital = Layout::kFsVital;
        constexpr float kFsBanner = Layout::kFsBanner;

        FMargin BoxTL(float InX, float InY, float InW, float InH) { return Layout::BoxTL(InX, InY, InW, InH); }
        FMargin BoxBL(float InX, float InBottom, float InW, float InH) {
            return Layout::BoxBL(InX, InBottom, InW, InH);
        }
        FMargin BoxBR(float InRight, float InBottom, float InW, float InH) {
            return Layout::BoxBR(InRight, InBottom, InW, InH);
        }
        FMargin BoxTC(float InTop, float InW, float InH, float InOx = 0.0f) {
            return Layout::BoxTC(InTop, InW, InH, InOx);
        }
        FMargin BoxBC(float InBottom, float InW, float InH, float InOx = 0.0f) {
            return Layout::BoxBC(InBottom, InW, InH, InOx);
        }
        FMargin BoxC(float InOx, float InOy, float InW, float InH) { return Layout::BoxC(InOx, InOy, InW, InH); }
        glm::vec2 MeasurePadded(const std::string& InText, float InScale, float InPadX = 8.0f, float InPadY = 6.0f) {
            return Layout::MeasurePadded(InText, InScale, InPadX, InPadY);
        }
        TRef<UButton> MakeButton(const std::string& InName, const std::string& InLabel, float InFont = kFsButton,
                                 float InMinW = 200.0f, float InMinH = 0.0f) {
            return Layout::MakeButton(InName, InLabel, InFont, InMinW, InMinH);
        }
        void PlaceButtonTL(UCanvasPanel& InRoot, const TRef<UButton>& InBtn, float InX, float InY) {
            Layout::PlaceButtonTL(InRoot, InBtn, InX, InY);
        }
        void PlaceButtonC(UCanvasPanel& InRoot, const TRef<UButton>& InBtn, float InOx, float InOy) {
            Layout::PlaceButtonC(InRoot, InBtn, InOx, InOy);
        }
        void PlaceTextTL(UCanvasPanel& InRoot, const TRef<UTextBlock>& InText, float InX, float InY,
                         float InMinW = 0.0f, float InMinH = 0.0f) {
            Layout::PlaceTextTL(InRoot, InText, InX, InY, InMinW, InMinH);
        }
        void PlaceTextTC(UCanvasPanel& InRoot, const TRef<UTextBlock>& InText, float InTop, float InMinW = 0.0f,
                         float InOx = 0.0f) {
            Layout::PlaceTextTC(InRoot, InText, InTop, InMinW, InOx);
        }
        void PlaceTextBL(UCanvasPanel& InRoot, const TRef<UTextBlock>& InText, float InX, float InBottom,
                         float InMinW = 0.0f) {
            Layout::PlaceTextBL(InRoot, InText, InX, InBottom, InMinW);
        }
        void PlaceTextBR(UCanvasPanel& InRoot, const TRef<UTextBlock>& InText, float InRight, float InBottom,
                         float InMinW = 0.0f) {
            Layout::PlaceTextBR(InRoot, InText, InRight, InBottom, InMinW);
        }
        void PlaceTextBC(UCanvasPanel& InRoot, const TRef<UTextBlock>& InText, float InBottom, float InMinW = 0.0f) {
            Layout::PlaceTextBC(InRoot, InText, InBottom, InMinW);
        }
        void PlaceTextC(UCanvasPanel& InRoot, const TRef<UTextBlock>& InText, float InOx, float InOy,
                        float InMinW = 0.0f, float InMinH = 0.0f) {
            Layout::PlaceTextC(InRoot, InText, InOx, InOy, InMinW, InMinH);
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
        // Room for ~14 roster lines at body scale.
        const float rosterH = MeasurePadded(std::string(14, '\n') + "x", kFsBody).y;
        Root->AddChild(RosterText, FAnchors::TopLeft(), BoxTL(leftX, y, 1160.0f, rosterH));
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
        Root->AddChild(CharacterLabel, FAnchors::TopLeft(),
                       BoxTL(charX + prevChar->GetSize().x + 12.0f, charY + (prevChar->GetSize().y - nameH) * 0.5f,
                             nameW, nameH));
        RefreshCharacterLabel();

        auto nextChar = MakeButton("LobbyNextChar", ">", kFsSub, 56.0f, 52.0f);
        nextChar->OnClicked.AddLambda([this]() { OnNextCharacter(); });
        PlaceButtonTL(*Root, nextChar, charX + prevChar->GetSize().x + 12.0f + nameW + 12.0f, charY);

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


} // namespace Leon
