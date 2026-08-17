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
