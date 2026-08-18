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


} // namespace Leon
