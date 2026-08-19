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
        ULeonTournamentGameInstance* GI() { return FLeonTournamentUILayout::GI(); }
        bool GamepadEdge(int InButton, bool& InOutWasDown) { return FLeonTournamentUILayout::GamepadEdge(InButton, InOutWasDown); }
        ALeonTournamentGameMode* GM(APlayerController* InPC) { return FLeonTournamentUILayout::GM(InPC); }
        ALeonTournamentGameState* GS(APlayerController* InPC) { return FLeonTournamentUILayout::GS(InPC); }
        bool IsClientWorld(APlayerController* InPC) { return FLeonTournamentUILayout::IsClientWorld(InPC); }
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

        FMargin BoxTL(float InX, float InY, float InW, float InH) { return FLeonTournamentUILayout::BoxTL(InX, InY, InW, InH); }
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
        FMargin BoxC(float InOx, float InOy, float InW, float InH) { return FLeonTournamentUILayout::BoxC(InOx, InOy, InW, InH); }
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
        ApplyViewportLayout();
    }

    void ULeonTournamentPauseWidget::ApplyViewportLayout() {
        if (!Root)
            return;
        SetSize(FLeonTournamentUILayout::ResolveViewportSize(Root.get()));
        FLeonTournamentUILayout::SyncResolutionScale(*Root, AppliedLayoutScale, AppliedViewport);
    }

    void ULeonTournamentPauseWidget::Tick(float InDeltaTime) {
        UUserWidget::Tick(InDeltaTime);
        const glm::vec2 vp = FLeonTournamentUILayout::ResolveViewportSize(Root.get());
        const float scale = FUILayout::LayoutScale(vp.x, vp.y);
        if (std::abs(scale - AppliedLayoutScale) > 0.001f || glm::length(vp - AppliedViewport) > 1.0f)
            ApplyViewportLayout();
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
