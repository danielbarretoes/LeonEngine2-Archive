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

        // Inter Bold is baked at 48px; scales below keep UI readable without oversized boxes.
        constexpr float kFsCaption = 0.32f;
        constexpr float kFsBody = 0.38f;
        constexpr float kFsLabel = 0.42f;
        constexpr float kFsButton = 0.42f;
        constexpr float kFsSub = 0.48f;
        constexpr float kFsTitle = 0.65f;
        constexpr float kFsHero = 0.78f;
        constexpr float kFsScore = 0.72f;
        constexpr float kFsTimer = 0.78f;
        constexpr float kFsVital = 0.85f;
        constexpr float kFsBanner = 1.05f;

        FMargin BoxTL(float InX, float InY, float InW, float InH) {
            return FMargin(InX, InY, -(InX + InW), -(InY + InH));
        }
        FMargin BoxBL(float InX, float InBottom, float InW, float InH) {
            return FMargin(InX, -(InBottom + InH), -(InX + InW), InBottom);
        }
        FMargin BoxBR(float InRight, float InBottom, float InW, float InH) {
            return FMargin(-(InRight + InW), -(InBottom + InH), InRight, InBottom);
        }
        FMargin BoxTC(float InTop, float InW, float InH, float InOx = 0.0f) {
            return FMargin(InOx - InW * 0.5f, InTop, -(InOx + InW * 0.5f), -(InTop + InH));
        }
        FMargin BoxBC(float InBottom, float InW, float InH, float InOx = 0.0f) {
            return FMargin(InOx - InW * 0.5f, -(InBottom + InH), -(InOx + InW * 0.5f), InBottom);
        }
        FMargin BoxC(float InOx, float InOy, float InW, float InH) {
            return FMargin(InOx - InW * 0.5f, InOy - InH * 0.5f, -(InOx + InW * 0.5f), -(InOy + InH * 0.5f));
        }

        glm::vec2 MeasurePadded(const std::string& InText, float InScale, float InPadX = 8.0f,
                                float InPadY = 6.0f) {
            const glm::vec2 m = FUIRenderer::MeasureString(InText, InScale);
            return {m.x + InPadX * 2.0f, m.y + InPadY * 2.0f};
        }

        TRef<UButton> MakeButton(const std::string& InName, const std::string& InLabel, float InFont = kFsButton,
                                 float InMinW = 200.0f, float InMinH = 0.0f) {
            FUIRenderer::Init();
            auto btn = std::make_shared<UButton>(InName);
            btn->SetNormalColor({0.12f, 0.18f, 0.30f, 0.95f});
            btn->SetHoveredColor({0.18f, 0.30f, 0.50f, 1.0f});
            btn->SetPressedColor({0.08f, 0.12f, 0.20f, 1.0f});
            auto label = std::make_shared<UTextBlock>(InName + "Label");
            label->SetText(InLabel);
            label->SetFontScale(InFont);
            label->SetColor({0.95f, 0.97f, 1.0f, 1.0f});
            label->SetJustification(ETextAlignment::Center);
            const glm::vec2 m = FUIRenderer::MeasureString(InLabel, InFont);
            label->SetSize(m);
            btn->SetContent(label);
            constexpr float padX = 36.0f;
            constexpr float padY = 14.0f;
            const float w = std::max(InMinW, m.x + padX * 2.0f);
            const float h = std::max(InMinH > 0.0f ? InMinH : (m.y + padY * 2.0f), m.y + padY * 2.0f);
            btn->SetSize({w, h});
            return btn;
        }

        void PlaceButtonTL(UCanvasPanel& InRoot, const TRef<UButton>& InBtn, float InX, float InY) {
            const glm::vec2 s = InBtn->GetSize();
            InRoot.AddChild(InBtn, FAnchors::TopLeft(), BoxTL(InX, InY, s.x, s.y));
        }
        void PlaceButtonC(UCanvasPanel& InRoot, const TRef<UButton>& InBtn, float InOx, float InOy) {
            const glm::vec2 s = InBtn->GetSize();
            InRoot.AddChild(InBtn, FAnchors::Center(), BoxC(InOx, InOy, s.x, s.y));
        }
        void PlaceTextTL(UCanvasPanel& InRoot, const TRef<UTextBlock>& InText, float InX, float InY,
                         float InMinW = 0.0f, float InMinH = 0.0f) {
            const glm::vec2 e = MeasurePadded(InText->GetText().empty() ? " " : InText->GetText(),
                                              InText->GetFontScale());
            const float w = std::max(InMinW, e.x);
            const float h = std::max(InMinH, e.y);
            InRoot.AddChild(InText, FAnchors::TopLeft(), BoxTL(InX, InY, w, h));
        }
        void PlaceTextTC(UCanvasPanel& InRoot, const TRef<UTextBlock>& InText, float InTop, float InMinW = 0.0f,
                         float InOx = 0.0f) {
            const glm::vec2 e = MeasurePadded(InText->GetText().empty() ? " " : InText->GetText(),
                                              InText->GetFontScale());
            const float w = std::max(InMinW, e.x);
            InRoot.AddChild(InText, FAnchors::TopCenter(), BoxTC(InTop, w, e.y, InOx));
        }
        void PlaceTextBL(UCanvasPanel& InRoot, const TRef<UTextBlock>& InText, float InX, float InBottom,
                         float InMinW = 0.0f) {
            const glm::vec2 e = MeasurePadded(InText->GetText().empty() ? " " : InText->GetText(),
                                              InText->GetFontScale());
            const float w = std::max(InMinW, e.x);
            InRoot.AddChild(InText, FAnchors::BottomLeft(), BoxBL(InX, InBottom, w, e.y));
        }
        void PlaceTextBR(UCanvasPanel& InRoot, const TRef<UTextBlock>& InText, float InRight, float InBottom,
                         float InMinW = 0.0f) {
            const glm::vec2 e = MeasurePadded(InText->GetText().empty() ? " " : InText->GetText(),
                                              InText->GetFontScale());
            const float w = std::max(InMinW, e.x);
            InRoot.AddChild(InText, FAnchors::BottomRight(), BoxBR(InRight, InBottom, w, e.y));
        }
        void PlaceTextBC(UCanvasPanel& InRoot, const TRef<UTextBlock>& InText, float InBottom,
                         float InMinW = 0.0f) {
            const glm::vec2 e = MeasurePadded(InText->GetText().empty() ? " " : InText->GetText(),
                                              InText->GetFontScale());
            const float w = std::max(InMinW, e.x);
            InRoot.AddChild(InText, FAnchors::BottomCenter(), BoxBC(InBottom, w, e.y));
        }
        void PlaceTextC(UCanvasPanel& InRoot, const TRef<UTextBlock>& InText, float InOx, float InOy,
                        float InMinW = 0.0f, float InMinH = 0.0f) {
            const glm::vec2 e = MeasurePadded(InText->GetText().empty() ? " " : InText->GetText(),
                                              InText->GetFontScale());
            const float w = std::max(InMinW, e.x);
            const float h = std::max(InMinH, e.y);
            InRoot.AddChild(InText, FAnchors::Center(), BoxC(InOx, InOy, w, h));
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

        constexpr float leftX = 72.0f;
        float y = 64.0f;

        auto title = std::make_shared<UTextBlock>("Title");
        title->SetText("LEON TOURNAMENT");
        title->SetFontScale(kFsHero);
        title->SetColor({0.90f, 0.93f, 1.0f, 1.0f});
        PlaceTextTL(*Root, title, leftX, y);
        y += MeasurePadded(title->GetText(), title->GetFontScale()).y + 4.0f;

        auto sub = std::make_shared<UTextBlock>("Sub");
        sub->SetText("2v2 Team Deathmatch");
        sub->SetFontScale(kFsBody);
        sub->SetColor({0.65f, 0.72f, 0.85f, 1.0f});
        PlaceTextTL(*Root, sub, leftX, y);
        y += MeasurePadded(sub->GetText(), sub->GetFontScale()).y + 28.0f;

        auto offline = MakeButton("Offline", "PLAY OFFLINE", kFsButton, 280.0f);
        offline->OnClicked.AddLambda([this]() { OnOffline(); });
        PlaceButtonTL(*Root, offline, leftX, y);
        y += offline->GetSize().y + 12.0f;

        auto animLab = MakeButton("AnimLab", "ANIM LAB", kFsButton, 280.0f);
        animLab->OnClicked.AddLambda([this]() { OnAnimLab(); });
        PlaceButtonTL(*Root, animLab, leftX, y);
        y += animLab->GetSize().y + 12.0f;

        auto host = MakeButton("Host", "HOST LAN", kFsButton, 280.0f);
        host->OnClicked.AddLambda([this]() { OnHostLan(); });
        PlaceButtonTL(*Root, host, leftX, y);
        y += host->GetSize().y + 12.0f;

        auto join = MakeButton("Join", "JOIN LAN", kFsButton, 280.0f);
        join->OnClicked.AddLambda([this]() { OnJoinLan(); });
        PlaceButtonTL(*Root, join, leftX, y);
        y += join->GetSize().y + 18.0f;

        auto ipLabel = std::make_shared<UTextBlock>("IpLabel");
        ipLabel->SetText("IP Address");
        ipLabel->SetFontScale(kFsCaption);
        ipLabel->SetColor({0.65f, 0.72f, 0.85f, 1.0f});
        PlaceTextTL(*Root, ipLabel, leftX, y);
        y += MeasurePadded(ipLabel->GetText(), ipLabel->GetFontScale()).y + 4.0f;

        AddressField = std::make_shared<UEditableText>("JoinAddress");
        auto* session = GI();
        AddressField->SetText(session && !session->GetJoinAddress().empty() ? session->GetJoinAddress()
                                                                            : std::string("127.0.0.1"));
        AddressField->SetHint("127.0.0.1");
        AddressField->OnTextChanged = [](const std::string& InText) {
            if (auto* inst = GI())
                inst->SetJoinAddress(InText.empty() ? "127.0.0.1" : InText);
        };
        const float fieldH = 44.0f;
        const float fieldW = 280.0f;
        AddressField->SetSize({fieldW, fieldH});
        Root->AddChild(AddressField, FAnchors::TopLeft(), BoxTL(leftX, y, fieldW, fieldH));
        y += fieldH + 20.0f;

        auto quit = MakeButton("Quit", "QUIT", kFsButton, 280.0f);
        quit->OnClicked.AddLambda([this]() { OnQuit(); });
        PlaceButtonTL(*Root, quit, leftX, y);

        constexpr float charX = 430.0f;
        float charY = 220.0f;

        auto charTitle = std::make_shared<UTextBlock>("CharTitle");
        charTitle->SetText("CHARACTER");
        charTitle->SetFontScale(kFsCaption);
        charTitle->SetColor({0.65f, 0.72f, 0.85f, 1.0f});
        PlaceTextTL(*Root, charTitle, charX, charY);
        charY += MeasurePadded(charTitle->GetText(), charTitle->GetFontScale()).y + 10.0f;

        auto prevChar = MakeButton("PrevChar", "<", kFsSub, 56.0f, 52.0f);
        prevChar->OnClicked.AddLambda([this]() { OnPrevCharacter(); });
        PlaceButtonTL(*Root, prevChar, charX, charY);

        CharacterLabel = std::make_shared<UTextBlock>("CharName");
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

        auto nextChar = MakeButton("NextChar", ">", kFsSub, 56.0f, 52.0f);
        nextChar->OnClicked.AddLambda([this]() { OnNextCharacter(); });
        PlaceButtonTL(*Root, nextChar, charX + prevChar->GetSize().x + 12.0f + nameW + 12.0f, charY);
        charY += prevChar->GetSize().y + 12.0f;

        auto charHint = std::make_shared<UTextBlock>("CharHint");
        charHint->SetText("Same Mixamo skeleton — shared anims");
        charHint->SetFontScale(kFsCaption);
        charHint->SetColor({0.55f, 0.62f, 0.75f, 1.0f});
        PlaceTextTL(*Root, charHint, charX, charY);

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

        constexpr float leftX = 56.0f;
        float y = 28.0f;

        TitleText = std::make_shared<UTextBlock>("LobbyTitle");
        TitleText->SetText("LOBBY  —  UP TO 12");
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

        const float topBarH = MeasurePadded("00:00", kFsTimer).y + MeasurePadded("TEAM DEATHMATCH", kFsBody).y + 18.0f;
        TopBar = std::make_shared<UImage>("TopBar");
        TopBar->SetTintColor({0.02f, 0.03f, 0.06f, 0.55f});
        Root->AddChild(TopBar, FAnchors::TopLeft(), BoxTL(0.0f, 0.0f, 1280.0f, topBarH));

        const float vitalH = MeasurePadded("999", kFsVital).y;
        const float labelH = MeasurePadded("HEALTH", kFsCaption).y;
        const float bottomPad = 18.0f;
        const float bottomBarH = labelH + vitalH + 28.0f;
        const float bottomBarW = 320.0f;

        BottomBarL = std::make_shared<UImage>("BottomBarL");
        BottomBarL->SetTintColor({0.02f, 0.04f, 0.05f, 0.62f});
        Root->AddChild(BottomBarL, FAnchors::BottomLeft(), BoxBL(24.0f, bottomPad, bottomBarW, bottomBarH));

        BottomBarR = std::make_shared<UImage>("BottomBarR");
        BottomBarR->SetTintColor({0.02f, 0.04f, 0.05f, 0.62f});
        Root->AddChild(BottomBarR, FAnchors::BottomRight(), BoxBR(24.0f, bottomPad, bottomBarW, bottomBarH));

        MatchLabel = std::make_shared<UTextBlock>("MatchLabel");
        MatchLabel->SetText("TEAM DEATHMATCH");
        MatchLabel->SetFontScale(kFsBody);
        MatchLabel->SetColor({0.72f, 0.80f, 0.92f, 0.85f});
        MatchLabel->SetJustification(ETextAlignment::Center);
        PlaceTextTC(*Root, MatchLabel, 10.0f, 280.0f);

        const float scoreTop = 10.0f + MeasurePadded("TEAM DEATHMATCH", kFsBody).y + 4.0f;

        Team1Text = std::make_shared<UTextBlock>("T1");
        Team1Text->SetText("0");
        Team1Text->SetFontScale(kFsScore);
        Team1Text->SetColor({1.0f, 0.42f, 0.32f, 1.0f});
        Team1Text->SetJustification(ETextAlignment::Right);
        PlaceTextTC(*Root, Team1Text, scoreTop, 80.0f, -140.0f);

        TimerText = std::make_shared<UTextBlock>("Timer");
        TimerText->SetText("00:00");
        TimerText->SetFontScale(kFsTimer);
        TimerText->SetColor({0.98f, 0.98f, 1.0f, 1.0f});
        TimerText->SetJustification(ETextAlignment::Center);
        PlaceTextTC(*Root, TimerText, scoreTop, 140.0f);

        Team2Text = std::make_shared<UTextBlock>("T2");
        Team2Text->SetText("0");
        Team2Text->SetFontScale(kFsScore);
        Team2Text->SetColor({0.38f, 0.62f, 1.0f, 1.0f});
        Team2Text->SetJustification(ETextAlignment::Left);
        PlaceTextTC(*Root, Team2Text, scoreTop, 80.0f, 140.0f);

        CrosshairText = std::make_shared<UTextBlock>("Crosshair");
        CrosshairText->SetText("");
        CrosshairText->SetFontScale(kFsLabel);
        CrosshairText->SetColor({0.95f, 0.97f, 1.0f, 0.0f});
        CrosshairText->SetJustification(ETextAlignment::Center);
        Root->AddChild(CrosshairText, FAnchors::Center(), BoxC(0.0f, 0.0f, 4.0f, 4.0f));

        auto centerDot = std::make_shared<UImage>("CrosshairDot");
        centerDot->SetTintColor({0.95f, 0.97f, 1.0f, 0.95f});
        Root->AddChild(centerDot, FAnchors::Center(), BoxC(0.0f, 0.0f, 3.0f, 3.0f));

        auto makeBar = [&](const char* name) {
            auto bar = std::make_shared<UImage>(name);
            bar->SetTintColor({0.95f, 0.97f, 1.0f, 0.9f});
            Root->AddChild(bar, FAnchors::Center(), BoxC(0.0f, 0.0f, 1.0f, 1.0f));
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
            Root->AddChild(seg, FAnchors::Center(), BoxC(0.0f, 0.0f, 1.0f, 1.0f));
            CrosshairRing[static_cast<size_t>(i)] = seg;
        }

        auto makeHit = [&](const char* name) {
            auto mark = std::make_shared<UImage>(name);
            mark->SetTintColor({1.0f, 0.85f, 0.15f, 1.0f});
            mark->SetVisibility(ESlateVisibility::Collapsed);
            Root->AddChild(mark, FAnchors::Center(), BoxC(0.0f, 0.0f, 1.0f, 1.0f));
            return mark;
        };
        HitMarkTL = makeHit("HM_TL");
        HitMarkTR = makeHit("HM_TR");
        HitMarkBL = makeHit("HM_BL");
        HitMarkBR = makeHit("HM_BR");

        HealthLabel = std::make_shared<UTextBlock>("HPLabel");
        HealthLabel->SetText("HEALTH");
        HealthLabel->SetFontScale(kFsCaption);
        HealthLabel->SetColor({0.55f, 0.95f, 0.65f, 0.85f});
        PlaceTextBL(*Root, HealthLabel, 44.0f, bottomPad + vitalH + 8.0f, 120.0f);

        HealthText = std::make_shared<UTextBlock>("HP");
        HealthText->SetText("100");
        HealthText->SetFontScale(kFsVital);
        HealthText->SetColor({0.45f, 1.0f, 0.55f, 1.0f});
        PlaceTextBL(*Root, HealthText, 44.0f, bottomPad + 10.0f, 160.0f);

        AmmoLabel = std::make_shared<UTextBlock>("AmmoLabel");
        AmmoLabel->SetText("AMMO");
        AmmoLabel->SetFontScale(kFsCaption);
        AmmoLabel->SetColor({0.85f, 0.88f, 0.95f, 0.85f});
        AmmoLabel->SetJustification(ETextAlignment::Right);
        PlaceTextBR(*Root, AmmoLabel, 44.0f, bottomPad + vitalH + 8.0f, 160.0f);

        AmmoText = std::make_shared<UTextBlock>("Ammo");
        AmmoText->SetText("30 / 30");
        AmmoText->SetFontScale(kFsScore);
        AmmoText->SetColor({0.95f, 0.97f, 1.0f, 1.0f});
        AmmoText->SetJustification(ETextAlignment::Right);
        PlaceTextBR(*Root, AmmoText, 44.0f, bottomPad + 10.0f, 200.0f);

        WeaponSlotsText = std::make_shared<UTextBlock>("WeaponSlots");
        WeaponSlotsText->SetText("[1]  2  3  4  5  6");
        WeaponSlotsText->SetFontScale(kFsBody);
        WeaponSlotsText->SetColor({0.78f, 0.86f, 1.0f, 0.95f});
        WeaponSlotsText->SetJustification(ETextAlignment::Right);
        PlaceTextBR(*Root, WeaponSlotsText, 44.0f, bottomPad + bottomBarH + 8.0f, 280.0f);

        StatusText = std::make_shared<UTextBlock>("Status");
        StatusText->SetText("RESPAWNING  -  FREE LOOK");
        StatusText->SetFontScale(kFsLabel);
        StatusText->SetColor({1.0f, 0.55f, 0.45f, 1.0f});
        StatusText->SetJustification(ETextAlignment::Center);
        StatusText->SetVisibility(ESlateVisibility::Collapsed);
        PlaceTextBC(*Root, StatusText, bottomPad + bottomBarH + 8.0f, 420.0f);

        KillText = std::make_shared<UTextBlock>("KillConfirm");
        KillText->SetText("KILL");
        KillText->SetFontScale(kFsHero);
        KillText->SetColor({1.0f, 0.82f, 0.25f, 1.0f});
        KillText->SetJustification(ETextAlignment::Center);
        PlaceTextC(*Root, KillText, 0.0f, -120.0f, 280.0f);
        KillText->SetVisibility(ESlateVisibility::Collapsed);

        BannerText = std::make_shared<UTextBlock>("Banner");
        BannerText->SetText("FIGHT!");
        BannerText->SetFontScale(kFsBanner);
        BannerText->SetColor({1.0f, 0.95f, 0.75f, 1.0f});
        BannerText->SetJustification(ETextAlignment::Center);
        BannerText->SetVisibility(ESlateVisibility::Collapsed);
        PlaceTextC(*Root, BannerText, 0.0f, 0.0f, 420.0f);

        DamageFlash = std::make_shared<UImage>("DamageFlash");
        DamageFlash->SetTintColor({0.85f, 0.08f, 0.08f, 0.22f});
        Root->AddChild(DamageFlash, FAnchors::Fill(), FMargin(0, 0, 0, 0));
        DamageFlash->SetVisibility(ESlateVisibility::Collapsed);

        HintText = std::make_shared<UTextBlock>("Hints");
        HintText->SetFontScale(kFsCaption);
        HintText->SetColor({0.78f, 0.86f, 0.96f, 0.92f});
        HintText->SetVisibility(ESlateVisibility::Collapsed);
        HintText->SetText(
            "V Camera   LMB Fire   RMB Scope   R Reload   Alt/C Dodge   Space x2 Double Jump   1-6 Weapons");
        PlaceTextTL(*Root, HintText, 36.0f, topBarH + 12.0f, 900.0f);

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
                if (MatchLabel)
                    MatchLabel->SetText("ANIMATION LAB");
                Team1Text->SetText("LAB");
                Team2Text->SetText(animLab->PrefersThirdPerson() ? "3RD" : "1ST");
                TimerText->SetText("--:--");
            } else if (gs) {
                if (MatchLabel) {
                    if (gs->GetMatchState() == ELeonTournamentMatchState::Starting)
                        MatchLabel->SetText("GET READY");
                    else if (gs->GetMatchState() == ELeonTournamentMatchState::Finished)
                        MatchLabel->SetText("MATCH OVER");
                    else
                        MatchLabel->SetText("TEAM DEATHMATCH");
                }
                Team1Text->SetText(std::to_string(gs->GetTeam1Kills()));
                Team2Text->SetText(std::to_string(gs->GetTeam2Kills()));
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
        auto* spc = dynamic_cast<ALeonTournamentPlayerController*>(OwningPlayer);
        const bool bDead = ch && ch->IsDeadFrozen();
        if (HealthText) {
            if (!ch || !health)
                HealthText->SetText("0");
            else if (health->IsDead() || bDead)
                HealthText->SetText("--");
            else
                HealthText->SetText(std::to_string(static_cast<int>(health->GetHealth())));
            if (health && !health->IsDead() && !bDead) {
                const float pct = health->GetMaxHealth() > 0.0f ? health->GetHealth() / health->GetMaxHealth() : 0.0f;
                if (pct < 0.3f)
                    HealthText->SetColor({1.0f, 0.35f, 0.3f, 1.0f});
                else if (pct < 0.6f)
                    HealthText->SetColor({1.0f, 0.85f, 0.35f, 1.0f});
                else
                    HealthText->SetColor({0.45f, 1.0f, 0.55f, 1.0f});
            } else {
                HealthText->SetColor({0.85f, 0.45f, 0.4f, 1.0f});
            }
        }
        if (AmmoText) {
            if (bLab && ch && !ch->GetWeapon()) {
                AmmoText->SetText("MELEE");
            } else if (!ch || !ch->GetWeapon() || bDead) {
                AmmoText->SetText("-- / --");
            } else if (ch->GetWeapon()->IsReloading()) {
                AmmoText->SetText("RELOAD");
            } else {
                AmmoText->SetText(std::to_string(ch->GetWeapon()->GetCurrentAmmo()) + " / " +
                                  std::to_string(ch->GetWeapon()->GetMagazineSize()));
            }
        }
        if (AmmoLabel) {
            if (ch && ch->GetWeapon() && !bDead)
                AmmoLabel->SetText(LeonTournamentWeaponName(ch->GetActiveWeaponId()));
            else
                AmmoLabel->SetText("AMMO");
        }
        if (WeaponSlotsText) {
            if (!ch || bLab || bDead) {
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
                WeaponSlotsText->SetText(slots);
            }
        }
        if (StatusText) {
            if (bDead) {
                StatusText->SetVisibility(ESlateVisibility::HitTestInvisible);
                StatusText->SetText("RESPAWNING  -  FREE LOOK");
            } else if (gs && gs->GetMatchState() == ELeonTournamentMatchState::Starting) {
                StatusText->SetVisibility(ESlateVisibility::HitTestInvisible);
                StatusText->SetText("ROUND STARTING");
            } else {
                StatusText->SetVisibility(ESlateVisibility::Collapsed);
            }
        }

        // Match countdown / FIGHT! banners + SFX
        if (spc && gs) {
            if (gs->GetMatchState() == ELeonTournamentMatchState::Starting) {
                bPlayedFightBanner = false;
                const int sec = static_cast<int>(std::ceil(std::max(0.0f, gs->GetCountdownRemaining())));
                if (sec > 0 && sec != LastCountdownSecond) {
                    LastCountdownSecond = sec;
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "%d", sec);
                    spc->PushBanner(buf, 0.85f, {1.0f, 0.95f, 0.85f, 1.0f});
                    UGameplayStatics::PlaySound2D("/Game/Audio/SFX_Countdown", 0.7f);
                }
            } else if (gs->GetMatchState() == ELeonTournamentMatchState::Playing) {
                if (!bPlayedFightBanner) {
                    bPlayedFightBanner = true;
                    LastCountdownSecond = -1;
                    spc->PushBanner("FIGHT!", 1.4f, {1.0f, 0.9f, 0.35f, 1.0f});
                    UGameplayStatics::PlaySound2D("/Game/Audio/SFX_MatchStart", 0.85f);
                }
            } else {
                LastCountdownSecond = -1;
                bPlayedFightBanner = false;
            }
        }

        if (HintText) {
            if (bLab) {
                HintText->SetVisibility(ESlateVisibility::HitTestInvisible);
                HintText->SetText(
                    "V Camera   LMB Fire   RMB Scope   R Reload   Alt/C Dodge   Space x2 Double Jump   1-6 Weapons");
            } else {
                HintText->SetVisibility(ESlateVisibility::Collapsed);
            }
        }
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
        if (KillText) {
            if (spc && spc->IsKillConfirmActive() && !spc->GetKillFeedText().empty()) {
                KillText->SetVisibility(ESlateVisibility::HitTestInvisible);
                KillText->SetText(spc->GetKillFeedText());
            } else {
                KillText->SetVisibility(ESlateVisibility::Collapsed);
            }
        }
        if (BannerText) {
            if (spc && spc->IsBannerActive() && !spc->GetBannerText().empty()) {
                BannerText->SetVisibility(ESlateVisibility::HitTestInvisible);
                BannerText->SetText(spc->GetBannerText());
                BannerText->SetColor(spc->GetBannerColor());
            } else {
                BannerText->SetVisibility(ESlateVisibility::Collapsed);
            }
        }
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
        Root->SetBackgroundColor({0.0f, 0.0f, 0.0f, 0.45f});

        const float titleH = MeasurePadded("SCOREBOARD", kFsTitle).y;
        const float headerH = MeasurePadded("PLAYER                        K    D    A", kFsCaption).y;
        const float rowsH = MeasurePadded(std::string(16, '\n') + "x", kFsBody).y;
        const float footerH = MeasurePadded("T1  99     T2  99", kFsCaption).y;
        const float panelPad = 28.0f;
        const float panelW = 880.0f;
        const float panelH = panelPad * 2.0f + titleH + 12.0f + headerH + 8.0f + rowsH + 12.0f + footerH;

        Panel = std::make_shared<UImage>("SBPanel");
        Panel->SetTintColor({0.04f, 0.05f, 0.09f, 0.92f});
        Root->AddChild(Panel, FAnchors::Center(), BoxC(0.0f, 0.0f, panelW, panelH));

        float y = -panelH * 0.5f + panelPad;
        TitleText = std::make_shared<UTextBlock>("SBTitle");
        TitleText->SetText("SCOREBOARD");
        TitleText->SetFontScale(kFsTitle);
        TitleText->SetColor({0.95f, 0.97f, 1.0f, 1.0f});
        TitleText->SetJustification(ETextAlignment::Center);
        PlaceTextC(*Root, TitleText, 0.0f, y + titleH * 0.5f, 360.0f);
        y += titleH + 12.0f;

        HeaderText = std::make_shared<UTextBlock>("SBHeader");
        HeaderText->SetText("PLAYER                        K    D    A");
        HeaderText->SetFontScale(kFsCaption);
        HeaderText->SetColor({0.65f, 0.74f, 0.90f, 0.95f});
        PlaceTextC(*Root, HeaderText, 0.0f, y + headerH * 0.5f, panelW - panelPad * 2.0f);
        y += headerH + 8.0f;

        RowsText = std::make_shared<UTextBlock>("SBRows");
        RowsText->SetFontScale(kFsBody);
        RowsText->SetColor({0.94f, 0.95f, 0.98f, 1.0f});
        RowsText->SetText(" ");
        Root->AddChild(RowsText, FAnchors::Center(), BoxC(0.0f, y + rowsH * 0.5f, panelW - panelPad * 2.0f, rowsH));
        y += rowsH + 12.0f;

        FooterText = std::make_shared<UTextBlock>("SBFooter");
        FooterText->SetText("HOLD  TAB");
        FooterText->SetFontScale(kFsCaption);
        FooterText->SetColor({0.55f, 0.62f, 0.75f, 0.85f});
        FooterText->SetJustification(ETextAlignment::Center);
        PlaceTextC(*Root, FooterText, 0.0f, y + footerH * 0.5f, 280.0f);

        SetWidgetTree(Root);
        SetSize({1280, 720});
        SetVisibility(ESlateVisibility::HitTestInvisible);
    }

    void ULeonTournamentScoreboardWidget::Tick(float InDeltaTime) {
        UUserWidget::Tick(InDeltaTime);
        auto* gs = GS(OwningPlayer);
        if (!gs || !RowsText)
            return;

        ALeonTournamentPlayerState* localPs = nullptr;
        if (OwningPlayer) {
            if (auto* pawn = OwningPlayer->GetPawn<ALeonTournamentCharacter>())
                localPs = pawn->GetPlayerState();
            if (!localPs)
                localPs = dynamic_cast<ALeonTournamentPlayerState*>(OwningPlayer->GetPlayerState());
        }

        std::ostringstream ss;
        auto appendGroup = [&](ELeonTournamentTeam team, const char* title) {
            ss << title << "\n";
            ss << "  PLAYER                      K    D    A\n";
            bool any = false;
            for (auto* ps : gs->GetSortedScoreboard()) {
                if (!ps || ps->GetTeam() != team)
                    continue;
                any = true;
                const bool bYou = localPs && ps == localPs;
                char line[160];
                std::snprintf(line, sizeof(line), "%s %-24s  %3d  %3d  %3d\n", bYou ? ">" : " ",
                              ps->GetPlayerName().c_str(), ps->GetKills(), ps->GetDeaths(), ps->GetAssists());
                ss << line;
            }
            if (!any)
                ss << "  --\n";
            ss << "\n";
        };
        appendGroup(ELeonTournamentTeam::Team1, "TEAM 1");
        appendGroup(ELeonTournamentTeam::Team2, "TEAM 2");
        {
            bool anyNone = false;
            for (auto* ps : gs->GetSortedScoreboard()) {
                if (ps && ps->GetTeam() == ELeonTournamentTeam::None) {
                    anyNone = true;
                    break;
                }
            }
            if (anyNone)
                appendGroup(ELeonTournamentTeam::None, "UNASSIGNED");
        }
        RowsText->SetText(ss.str());
        if (FooterText) {
            char foot[64];
            std::snprintf(foot, sizeof(foot), "T1  %d     T2  %d", gs->GetTeam1Kills(), gs->GetTeam2Kills());
            FooterText->SetText(foot);
        }
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

        constexpr float leftX = 72.0f;
        float y = 72.0f;

        ResultText = std::make_shared<UTextBlock>("Result");
        ResultText->SetText("WIN");
        ResultText->SetFontScale(kFsHero);
        PlaceTextTL(*Root, ResultText, leftX, y, 400.0f);
        y += MeasurePadded("WIN", kFsHero).y + 24.0f;

        StatsText = std::make_shared<UTextBlock>("Stats");
        StatsText->SetFontScale(kFsSub);
        StatsText->SetText("TEAM 1  0     TEAM 2  0\n\nYOU  K 0  D 0  A 0");
        const float statsH = MeasurePadded(StatsText->GetText(), kFsSub).y + 40.0f;
        Root->AddChild(StatsText, FAnchors::TopLeft(), BoxTL(leftX, y, 900.0f, statsH));
        y += statsH + 32.0f;

        auto back = MakeButton("Return", "RETURN TO MENU", kFsButton, 280.0f);
        back->OnClicked.AddLambda([this]() { OnReturn(); });
        PlaceButtonTL(*Root, back, leftX, y);

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
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_UIClick", 0.5f);
        if (auto* gm = GM(OwningPlayer))
            gm->ReturnToMenu();
    }

    ULeonTournamentPauseWidget::ULeonTournamentPauseWidget(const std::string& InName) : UUserWidget(InName) {}
    void ULeonTournamentPauseWidget::Construct() {
        Build();
    }

    void ULeonTournamentPauseWidget::Build() {
        FUIRenderer::Init();
        Root = std::make_shared<UCanvasPanel>("PauseRoot");
        Root->SetSize({1280, 720});
        Root->SetBackgroundColor({0.0f, 0.0f, 0.0f, 0.62f});

        auto resume = MakeButton("Resume", "RESUME", kFsButton, 280.0f);
        auto leave = MakeButton("Leave", "LEAVE TO MENU", kFsButton, 280.0f);
        const float titleH = MeasurePadded("PAUSED", kFsTitle).y;
        const float hintH = MeasurePadded("ESC to resume", kFsCaption).y;
        const float gap = 14.0f;
        const float panelPad = 36.0f;
        const float panelW = std::max(320.0f, leave->GetSize().x + panelPad * 2.0f);
        const float panelH = panelPad * 2.0f + titleH + 8.0f + hintH + 20.0f + resume->GetSize().y + gap +
                             leave->GetSize().y;

        auto panel = std::make_shared<UImage>("PausePanel");
        panel->SetTintColor({0.05f, 0.06f, 0.10f, 0.96f});
        Root->AddChild(panel, FAnchors::Center(), BoxC(0.0f, 0.0f, panelW, panelH));

        float y = -panelH * 0.5f + panelPad;
        auto title = std::make_shared<UTextBlock>("PauseTitle");
        title->SetText("PAUSED");
        title->SetFontScale(kFsTitle);
        title->SetColor({0.95f, 0.97f, 1.0f, 1.0f});
        title->SetJustification(ETextAlignment::Center);
        PlaceTextC(*Root, title, 0.0f, y + titleH * 0.5f, panelW - panelPad * 2.0f);
        y += titleH + 8.0f;

        auto hint = std::make_shared<UTextBlock>("PauseHint");
        hint->SetText("ESC to resume");
        hint->SetFontScale(kFsCaption);
        hint->SetColor({0.65f, 0.72f, 0.85f, 0.9f});
        hint->SetJustification(ETextAlignment::Center);
        PlaceTextC(*Root, hint, 0.0f, y + hintH * 0.5f, panelW - panelPad * 2.0f);
        y += hintH + 20.0f;

        resume->OnClicked.AddLambda([this]() { OnResume(); });
        PlaceButtonC(*Root, resume, 0.0f, y + resume->GetSize().y * 0.5f);
        y += resume->GetSize().y + gap;

        leave->OnClicked.AddLambda([this]() { OnLeave(); });
        PlaceButtonC(*Root, leave, 0.0f, y + leave->GetSize().y * 0.5f);

        SetWidgetTree(Root);
        SetSize({1280, 720});
        SetVisibility(ESlateVisibility::Collapsed);
    }

    void ULeonTournamentPauseWidget::Tick(float InDeltaTime) {
        UUserWidget::Tick(InDeltaTime);
        if (GamepadEdge(GamepadButton::A, bPadAWasDown))
            OnResume();
        if (GamepadEdge(GamepadButton::B, bPadBWasDown))
            OnLeave();
    }

    void ULeonTournamentPauseWidget::OnResume() {
        if (auto* spc = dynamic_cast<ALeonTournamentPlayerController*>(OwningPlayer))
            spc->SetPauseMenuOpen(false);
    }

    void ULeonTournamentPauseWidget::OnLeave() {
        if (auto* spc = dynamic_cast<ALeonTournamentPlayerController*>(OwningPlayer))
            spc->SetPauseMenuOpen(false);
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_UIClick", 0.5f);
        if (auto* gm = GM(OwningPlayer))
            gm->ReturnToMenu();
    }

} // namespace Leon
