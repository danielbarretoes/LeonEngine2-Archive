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


} // namespace Leon
