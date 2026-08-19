#include "ULeonTournamentWidgets.hpp"
#include "FLeonTournamentUILayout.hpp"
#include "FLeonTournamentUITheme.hpp"
#include "ALeonTournamentHUD.hpp"
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

    ULeonTournamentMatchEndWidget::ULeonTournamentMatchEndWidget(const std::string& InName) : UUserWidget(InName) {}
    void ULeonTournamentMatchEndWidget::Construct() {
        Build();
    }

    void ULeonTournamentMatchEndWidget::Build() {
        FUIRenderer::Init();
        Root = std::make_shared<UCanvasPanel>("EndRoot");
        Root->SetSize({1280, 720});
        Root->SetBackgroundColor(FLeonTournamentUITheme::DimOverlay);

        auto dim = FLeonTournamentUILayout::MakePanel("EndDim", FLeonTournamentUITheme::DimOverlay);
        auto panel = FLeonTournamentUILayout::MakePanel("EndPanel", FLeonTournamentUITheme::PanelBgStrong);
        auto accent = FLeonTournamentUILayout::MakeAccentStrip("EndAccent", true);

        ResultText = std::make_shared<UTextBlock>("Result");
        ResultText->SetText("WIN");
        ResultText->SetFontScale(kFsHero);
        FLeonTournamentUILayout::StyleHero(*ResultText);
        ResultText->SetJustification(ETextAlignment::Center);

        StatsText = std::make_shared<UTextBlock>("Stats");
        StatsText->SetFontScale(kFsBody);
        FLeonTournamentUILayout::StyleBody(*StatsText);
        StatsText->SetText("TEAM 1  0     TEAM 2  0\n\nYOU  K 0  D 0  A 0");
        StatsText->SetJustification(ETextAlignment::Center);

        auto back = FLeonTournamentUILayout::MakeThemedButton("Return", "RETURN TO MENU", kFsButton, 260.0f, 44.0f);
        back->OnClicked.AddLambda([this]() { OnReturn(); });

        RematchButton = FLeonTournamentUILayout::MakeThemedButton("Rematch", "PLAY AGAIN", kFsButton, 260.0f, 44.0f);
        RematchButton->OnClicked.AddLambda([this]() { OnRematch(); });

        const float titleH = MeasurePadded("WIN", kFsHero).y;
        const float statsH = MeasurePadded(StatsText->GetText(), kFsBody).y + 24.0f;
        const float panelPad = 40.0f;
        const float panelW = 720.0f;
        const float panelH = panelPad * 2.0f + titleH + 16.0f + statsH + 24.0f + RematchButton->GetSize().y;

        FLeonTournamentUILayout::ApplyModalLayout(*Root, dim, panel, accent, panelW, panelH);

        float y = -panelH * 0.5f + panelPad;
        PlaceTextC(*Root, ResultText, 0.0f, y + titleH * 0.5f, panelW - panelPad * 2.0f);
        y += titleH + 16.0f;
        PlaceTextC(*Root, StatsText, 0.0f, y + statsH * 0.5f, panelW - panelPad * 2.0f);
        y += statsH + 24.0f;
        PlaceButtonC(*Root, back, -140.0f, y + back->GetSize().y * 0.5f);
        PlaceButtonC(*Root, RematchButton, 140.0f, y + RematchButton->GetSize().y * 0.5f);

        SetWidgetTree(Root);
        SetSize({1280, 720});
        ApplyViewportLayout();
    }

    void ULeonTournamentMatchEndWidget::ApplyViewportLayout() {
        if (!Root)
            return;
        SetSize(FLeonTournamentUILayout::ResolveViewportSize(Root.get()));
        FLeonTournamentUILayout::SyncResolutionScale(*Root, AppliedLayoutScale, AppliedViewport);
    }

    void ULeonTournamentMatchEndWidget::Tick(float InDeltaTime) {
        UUserWidget::Tick(InDeltaTime);
        const glm::vec2 vp = FLeonTournamentUILayout::ResolveViewportSize(Root.get());
        const float scale = FUILayout::LayoutScale(vp.x, vp.y);
        if (std::abs(scale - AppliedLayoutScale) > 0.001f || glm::length(vp - AppliedViewport) > 1.0f)
            ApplyViewportLayout();
        if (GamepadEdge(GamepadButton::A, bPadAWasDown) || GamepadEdge(GamepadButton::Start, bPadStartWasDown))
            OnRematch();
        if (GamepadEdge(GamepadButton::B, bPadBWasDown))
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
        ResultText->SetColor(FLeonTournamentUITheme::ResultColor(result));

        std::ostringstream ss;
        ss << "TEAM 1  " << gs->GetTeam1Kills() << "     TEAM 2  " << gs->GetTeam2Kills() << "\n";
        for (auto* ps : gs->GetSortedScoreboard()) {
            if (!ps)
                continue;
            ss << "\n" << ps->GetPlayerName() << "  " << static_cast<int>(ps->GetScore()) << "  K " << ps->GetKills()
               << "  D " << ps->GetDeaths() << "  A " << ps->GetAssists();
        }
        if (StatsText)
            StatsText->SetText(ss.str());
    }

    void ULeonTournamentMatchEndWidget::OnReturn() {
        if (IsClientWorld(OwningPlayer))
            return;
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_UIClick", 0.5f);
        if (auto* hud = OwningPlayer ? dynamic_cast<ALeonTournamentHUD*>(OwningPlayer->GetHUD()) : nullptr)
            hud->ShowLoadingOverlay("RETURNING TO MENU...");
        if (auto* gm = GM(OwningPlayer))
            gm->ReturnToMenu();
    }

    void ULeonTournamentMatchEndWidget::OnRematch() {
        if (IsClientWorld(OwningPlayer))
            return;
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_UIClick", 0.5f);
        if (auto* gm = GM(OwningPlayer))
            gm->RequestRematch();
    }


} // namespace Leon
