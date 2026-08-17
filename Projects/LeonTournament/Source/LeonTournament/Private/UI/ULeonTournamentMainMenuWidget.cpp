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
        charHint->SetText("Same Mixamo skeleton â€” shared anims");
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


} // namespace Leon
