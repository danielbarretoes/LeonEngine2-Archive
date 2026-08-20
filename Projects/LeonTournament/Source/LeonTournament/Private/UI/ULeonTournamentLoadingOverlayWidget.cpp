#include "ULeonTournamentWidgets.hpp"
#include "FLeonTournamentUILayout.hpp"
#include "FLeonTournamentUITheme.hpp"
#include "UMG/FUIRenderer.hpp"
#include "UMG/FUILayout.hpp"

#include <cmath>

namespace Leon {

    namespace {
        constexpr float kFsStatus = FLeonTournamentUILayout::kFsCaption;
        FMargin BoxC(float InOx, float InOy, float InW, float InH) {
            return FLeonTournamentUILayout::BoxC(InOx, InOy, InW, InH);
        }
    } // namespace

    ULeonTournamentLoadingOverlayWidget::ULeonTournamentLoadingOverlayWidget(const std::string& InName)
        : UUserWidget(InName) {}

    void ULeonTournamentLoadingOverlayWidget::Construct() {
        Build();
    }

    void ULeonTournamentLoadingOverlayWidget::Build() {
        FUIRenderer::Init();
        Root = std::make_shared<UCanvasPanel>("LoadingRoot");
        Root->SetSize({1280, 720});
        Root->SetBackgroundColor(FLeonTournamentUITheme::VoidBg);

        Spinner = std::make_shared<ULoadingSpinner>("LoadingSpinner");
        Spinner->SetSize({88.0f, 88.0f});
        Spinner->SetDotRadius(5.0f);
        Spinner->SetSpinSpeed(5.5f);
        Root->AddChild(Spinner, FAnchors::Center(), BoxC(0.0f, -24.0f, 88.0f, 88.0f));

        StatusText = std::make_shared<UTextBlock>("LoadingStatus");
        StatusText->SetText("LOADING...");
        StatusText->SetFontScale(kFsStatus);
        StatusText->SetColor(FLeonTournamentUITheme::TextAccent);
        StatusText->SetJustification(ETextAlignment::Center);
        FLeonTournamentUILayout::PlaceTextC(*Root, StatusText, 0.0f, 56.0f, 420.0f);

        SetWidgetTree(Root);
        SetSize({1280, 720});
        SetVisibility(ESlateVisibility::HitTestInvisible);
        ApplyViewportLayout();
    }

    void ULeonTournamentLoadingOverlayWidget::ApplyViewportLayout() {
        if (!Root)
            return;
        SetSize(FLeonTournamentUILayout::ResolveViewportSize(Root.get()));
        FLeonTournamentUILayout::SyncResolutionScale(*Root, AppliedLayoutScale, AppliedViewport);
    }

    void ULeonTournamentLoadingOverlayWidget::SetStatusText(const std::string& InText) {
        if (StatusText)
            StatusText->SetText(InText.empty() ? "LOADING..." : InText);
    }

    void ULeonTournamentLoadingOverlayWidget::Tick(float InDeltaTime) {
        UUserWidget::Tick(InDeltaTime);
        const glm::vec2 vp = FLeonTournamentUILayout::ResolveViewportSize(Root.get());
        const float scale = FUILayout::LayoutScale(vp.x, vp.y);
        if (std::abs(scale - AppliedLayoutScale) > 0.001f || glm::length(vp - AppliedViewport) > 1.0f)
            ApplyViewportLayout();
    }

} // namespace Leon
