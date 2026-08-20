#include "ULeonTournamentWidgets.hpp"
#include "FLeonTournamentUILayout.hpp"
#include "FLeonTournamentUITheme.hpp"
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
#include "UMG/UTableView.hpp"
#include "Renderer/FPerspectiveCamera.hpp"

#include <algorithm>
#include <cstdio>
#include <vector>

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

    ULeonTournamentScoreboardWidget::ULeonTournamentScoreboardWidget(const std::string& InName) : UUserWidget(InName) {}
    void ULeonTournamentScoreboardWidget::Construct() {
        Build();
    }

    void ULeonTournamentScoreboardWidget::Build() {
        FUIRenderer::Init();
        Root = std::make_shared<UCanvasPanel>("SBRoot");
        Root->SetSize({1280, 720});
        Root->SetBackgroundColor(FLeonTournamentUITheme::DimOverlay);

        const float titleH = MeasurePadded("SCOREBOARD", kFsTitle).y;
        const float tableHeaderH = FUIRenderer::MeasureString("PLAYER", kFsCaption).y + 6.0f;
        const float tableRowH = FUIRenderer::MeasureString("X", kFsBody).y + 2.0f;
        const float tableSectionH = FUIRenderer::MeasureString("TEAM", kFsCaption).y + 4.0f;
        constexpr int kEstimatedDataRows = 10;
        constexpr int kEstimatedSections = 2;
        const float tableH = tableHeaderH + kEstimatedSections * tableSectionH + kEstimatedDataRows * tableRowH + 8.0f;
        const float footerH = MeasurePadded("T1  99     T2  99", kFsCaption).y;
        const float panelPad = 28.0f;
        const float panelW = 880.0f;
        const float panelH = panelPad * 2.0f + titleH + 12.0f + tableH + 12.0f + footerH;

        Panel = FLeonTournamentUILayout::MakePanel("SBPanel", FLeonTournamentUITheme::PanelBgStrong);
        Root->AddChild(Panel, FAnchors::Center(), BoxC(0.0f, 0.0f, panelW, panelH));
        auto accent = FLeonTournamentUILayout::MakeAccentStrip("SBAccent", true);
        Root->AddChild(accent, FAnchors::Center(), BoxC(-panelW * 0.5f + 2.0f, 0.0f, 4.0f, panelH));

        float y = -panelH * 0.5f + panelPad;
        TitleText = std::make_shared<UTextBlock>("SBTitle");
        TitleText->SetText("SCOREBOARD");
        TitleText->SetFontScale(kFsTitle);
        FLeonTournamentUILayout::StyleAccent(*TitleText);
        TitleText->SetJustification(ETextAlignment::Center);
        PlaceTextC(*Root, TitleText, 0.0f, y + titleH * 0.5f, 360.0f);
        y += titleH + 12.0f;

        ScoreTable = std::make_shared<UTableView>("SBTable");
        ScoreTable->SetFontScale(kFsBody);
        ScoreTable->SetHeaderFontScale(kFsCaption);
        ScoreTable->SetRowHeight(tableRowH);
        ScoreTable->SetHeaderHeight(tableHeaderH);
        ScoreTable->SetSectionHeight(tableSectionH);
        ScoreTable->SetColumns({{"PLAYER", 3.2f, ETextAlignment::Left},
                                {"SCORE", 1.0f, ETextAlignment::Right},
                                {"K", 0.7f, ETextAlignment::Center},
                                {"D", 0.7f, ETextAlignment::Center},
                                {"A", 0.7f, ETextAlignment::Center}});
        FLeonTournamentUILayout::ApplyScoreboardTableTheme(*ScoreTable);
        Root->AddChild(ScoreTable, FAnchors::Center(), BoxC(0.0f, y + tableH * 0.5f, panelW - panelPad * 2.0f, tableH));
        y += tableH + 12.0f;

        FooterText = std::make_shared<UTextBlock>("SBFooter");
        FooterText->SetText("HOLD  TAB");
        FooterText->SetFontScale(kFsCaption);
        FLeonTournamentUILayout::StyleCaption(*FooterText);
        FooterText->SetJustification(ETextAlignment::Center);
        PlaceTextC(*Root, FooterText, 0.0f, y + footerH * 0.5f, 280.0f);

        SetWidgetTree(Root);
        SetSize({1280, 720});
        SetVisibility(ESlateVisibility::HitTestInvisible);
        ApplyViewportLayout();
    }

    void ULeonTournamentScoreboardWidget::ApplyViewportLayout() {
        if (!Root)
            return;
        SetSize(FLeonTournamentUILayout::ResolveViewportSize(Root.get()));
        FLeonTournamentUILayout::SyncResolutionScale(*Root, AppliedLayoutScale, AppliedViewport);
    }

    void ULeonTournamentScoreboardWidget::Tick(float InDeltaTime) {
        UUserWidget::Tick(InDeltaTime);
        const glm::vec2 vp = FLeonTournamentUILayout::ResolveViewportSize(Root.get());
        const float scale = FUILayout::LayoutScale(vp.x, vp.y);
        if (std::abs(scale - AppliedLayoutScale) > 0.001f || glm::length(vp - AppliedViewport) > 1.0f)
            ApplyViewportLayout();
        // Unreal: scoreboard is a HUD view of GameState.PlayerArray + PlayerState (not GameMode).
        auto* gs = GS(OwningPlayer);
        if (!gs || !ScoreTable)
            return;

        auto* localPs =
            OwningPlayer ? dynamic_cast<ALeonTournamentPlayerState*>(OwningPlayer->GetPlayerState()) : nullptr;
        const auto ranked = gs->GetSortedScoreboard();

        std::vector<FTableRow> rows;
        auto appendGroup = [&](ELeonTournamentTeam team, const char* title) {
            rows.push_back({ETableRowKind::SectionHeader, {}, title, false, FLeonTournamentUITheme::TeamColor(team)});
            bool any = false;
            for (auto* ps : ranked) {
                if (!ps || ps->GetTeam() != team)
                    continue;
                any = true;
                const bool bYou = localPs && ps == localPs;
                char player[96];
                char score[16];
                char kills[8];
                char deaths[8];
                char assists[8];
                std::snprintf(player, sizeof(player), "%s%s", bYou ? "> " : "", ps->GetPlayerName().c_str());
                std::snprintf(score, sizeof(score), "%.0f", ps->GetScore());
                std::snprintf(kills, sizeof(kills), "%d", ps->GetKills());
                std::snprintf(deaths, sizeof(deaths), "%d", ps->GetDeaths());
                std::snprintf(assists, sizeof(assists), "%d", ps->GetAssists());
                rows.push_back({ETableRowKind::Data, {player, score, kills, deaths, assists}, {}, bYou});
            }
            if (!any)
                rows.push_back({ETableRowKind::Data, {"--", "-", "-", "-", "-"}, {}, false});
        };
        appendGroup(ELeonTournamentTeam::Team1, "TEAM 1");
        appendGroup(ELeonTournamentTeam::Team2, "TEAM 2");
        bool anyNone = false;
        for (auto* ps : ranked) {
            if (ps && ps->GetTeam() == ELeonTournamentTeam::None) {
                anyNone = true;
                break;
            }
        }
        if (anyNone)
            appendGroup(ELeonTournamentTeam::None, "UNASSIGNED");
        ScoreTable->SetRows(rows);
        if (FooterText) {
            char foot[64];
            std::snprintf(foot, sizeof(foot), "T1  %d     T2  %d", gs->GetTeam1Kills(), gs->GetTeam2Kills());
            FooterText->SetText(foot);
        }
    }

} // namespace Leon
