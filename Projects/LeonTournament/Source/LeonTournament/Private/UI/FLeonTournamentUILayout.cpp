#include "FLeonTournamentUILayout.hpp"
#include "FLeonTournamentTypes.hpp"
#include "ULeonTournamentGameInstance.hpp"
#include "ALeonTournamentGameMode.hpp"
#include "ALeonTournamentGameState.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Engine/UEngine.hpp"
#include "Engine/UWorld.hpp"
#include "UMG/UWidget.hpp"
#include "UMG/UImage.hpp"
#include "UMG/UButton.hpp"
#include "UMG/UTextBlock.hpp"
#include "UMG/UCanvasPanel.hpp"
#include "UMG/UTableView.hpp"
#include "UMG/FUIRenderer.hpp"

#include <algorithm>
#include <glm/glm.hpp>

namespace Leon {

    TRef<UButton> FLeonTournamentUILayout::MakeThemedButton(const std::string& InName, const std::string& InLabel,
                                                          float InFont, float InMinW, float InMinH) {
        auto btn = MakeButton(InName, InLabel, InFont, InMinW, InMinH);
        if (btn)
            ApplyThemedButtonColors(*btn, false);
        return btn;
    }

    TRef<UImage> FLeonTournamentUILayout::MakePanel(const std::string& InName, const glm::vec4& InTint) {
        auto panel = std::make_shared<UImage>(InName);
        panel->SetTintColor(InTint);
        return panel;
    }

    TRef<UImage> FLeonTournamentUILayout::MakeAccentStrip(const std::string& InName, bool bVertical) {
        auto strip = std::make_shared<UImage>(InName);
        strip->SetTintColor(FLeonTournamentUITheme::AccentOrange);
        strip->SetSize(bVertical ? glm::vec2{4.0f, 64.0f} : glm::vec2{64.0f, 3.0f});
        return strip;
    }

    void FLeonTournamentUILayout::StyleTitle(UTextBlock& InText) {
        InText.SetColor(FLeonTournamentUITheme::TextPrimary);
    }

    void FLeonTournamentUILayout::StyleHero(UTextBlock& InText) {
        InText.SetColor(FLeonTournamentUITheme::TextPrimary);
    }

    void FLeonTournamentUILayout::StyleCaption(UTextBlock& InText) {
        InText.SetColor(FLeonTournamentUITheme::TextSecondary);
    }

    void FLeonTournamentUILayout::StyleBody(UTextBlock& InText) {
        InText.SetColor(FLeonTournamentUITheme::TextSecondary);
    }

    void FLeonTournamentUILayout::StyleAccent(UTextBlock& InText) {
        InText.SetColor(FLeonTournamentUITheme::TextAccent);
    }

    void FLeonTournamentUILayout::ApplyThemedButtonColors(UButton& InButton, bool bSelected) {
        if (bSelected) {
            InButton.SetNormalColor(FLeonTournamentUITheme::BtnSelected);
            InButton.SetHoveredColor(FLeonTournamentUITheme::BtnSelected);
            InButton.SetPressedColor(FLeonTournamentUITheme::BtnPressed);
        } else {
            InButton.SetNormalColor(FLeonTournamentUITheme::BtnNormal);
            InButton.SetHoveredColor(FLeonTournamentUITheme::BtnHover);
            InButton.SetPressedColor(FLeonTournamentUITheme::BtnPressed);
        }
    }

    void FLeonTournamentUILayout::ApplyScoreboardTableTheme(UTableView& InTable) {
        InTable.SetBackgroundColor({FLeonTournamentUITheme::PanelBg.r, FLeonTournamentUITheme::PanelBg.g,
                                    FLeonTournamentUITheme::PanelBg.b, 0.55f});
        InTable.SetHeaderBackgroundColor(FLeonTournamentUITheme::HudBarBg);
        InTable.SetRowBackgroundColor({0.04f, 0.05f, 0.08f, 0.72f});
        InTable.SetAlternateRowBackgroundColor({0.06f, 0.07f, 0.11f, 0.72f});
        InTable.SetHighlightBackgroundColor({FLeonTournamentUITheme::AccentOrange.r * 0.22f,
                                             FLeonTournamentUITheme::AccentOrange.g * 0.22f,
                                             FLeonTournamentUITheme::AccentOrange.b * 0.22f, 0.92f});
        InTable.SetSectionBackgroundColor(FLeonTournamentUITheme::HudBarBg);
        InTable.SetBorderColor({FLeonTournamentUITheme::AccentOrange.r, FLeonTournamentUITheme::AccentOrange.g,
                                FLeonTournamentUITheme::AccentOrange.b, 0.35f});
        InTable.SetHeaderTextColor(FLeonTournamentUITheme::TextSecondary);
        InTable.SetRowTextColor(FLeonTournamentUITheme::TextPrimary);
        InTable.SetSectionTextColor(FLeonTournamentUITheme::TextAccent);
    }

    void FLeonTournamentUILayout::ApplyModalLayout(UCanvasPanel& InRoot, const TRef<UImage>& InDim,
                                                    const TRef<UImage>& InPanel, const TRef<UImage>& InAccent,
                                                    float InPanelW, float InPanelH) {
        auto place = [&](const TRef<UWidget>& widget, const FAnchors& anchors, const FMargin& offsets) {
            if (!widget)
                return;
            if (!InRoot.SetChildLayout(widget, anchors, offsets))
                InRoot.AddChild(widget, anchors, offsets);
        };
        place(InDim, FAnchors::Fill(), FMargin(0.0f, 0.0f, 0.0f, 0.0f));
        place(InPanel, FAnchors::Center(), BoxC(0.0f, 0.0f, InPanelW, InPanelH));
        if (InAccent)
            place(InAccent, FAnchors::Center(), BoxC(-InPanelW * 0.5f + 2.0f, 0.0f, 4.0f, InPanelH));
    }

    ULeonTournamentGameInstance* FLeonTournamentUILayout::GI() {
        return UEngine::HasInstance()
                   ? dynamic_cast<ULeonTournamentGameInstance*>(UEngine::Get().GetGameInstance().get())
                   : nullptr;
    }

    ALeonTournamentGameMode* FLeonTournamentUILayout::GM(APlayerController* InPC) {
        UWorld* world = InPC ? InPC->GetWorld() : nullptr;
        return world ? dynamic_cast<ALeonTournamentGameMode*>(world->GetGameMode()) : nullptr;
    }

    ALeonTournamentGameState* FLeonTournamentUILayout::GS(APlayerController* InPC) {
        UWorld* world = InPC ? InPC->GetWorld() : nullptr;
        return world ? dynamic_cast<ALeonTournamentGameState*>(world->GetGameState()) : nullptr;
    }

    bool FLeonTournamentUILayout::IsClientWorld(APlayerController* InPC) {
        UWorld* world = InPC ? InPC->GetWorld() : nullptr;
        return world && world->GetNetMode() == ENetMode::Client;
    }

    void FLeonTournamentUILayout::ApplyMenuRailLayout(UCanvasPanel& InRoot, const TRef<UWidget>& InPanel,
                                                      const TRef<UButton>& InPrev, const TRef<UTextBlock>& InLabel,
                                                      const TRef<UButton>& InNext, bool bLobby, float InScale) {
        const glm::vec2 vp = ResolveViewportSize(&InRoot);
        InRoot.SetSize(vp);
        const float s = InScale > 1.0e-4f ? InScale : LayoutScale(vp.x, vp.y);
        const float designPanel = bLobby ? kLeonTournamentLobbyPanelDesignWidth : kLeonTournamentMenuPanelDesignWidth;
        const float panelW = std::min(designPanel * s, vp.x * 0.62f);
        auto place = [&](const TRef<UWidget>& widget, const FAnchors& anchors, const FMargin& offsets) {
            if (!widget)
                return;
            if (!InRoot.SetChildLayout(widget, anchors, offsets))
                InRoot.AddChild(widget, anchors, offsets);
        };
        place(InPanel, FAnchors::LeftStretch(), BoxLeftStretch(0.0f, panelW));

        const float accentW = 5.0f * s;
        auto accent = MakeAccentStrip("MenuAccent", false);
        accent->SetSize({accentW, vp.y});
        place(accent, FAnchors::LeftStretch(), BoxLeftStretch(panelW - accentW * 0.5f, accentW));

        const float inset = 48.0f * s;
        const float gap = 12.0f * s;
        const float nameW = 220.0f * s;
        const float prevW = InPrev ? InPrev->GetSize().x : 56.0f * s;
        const float prevH = InPrev ? InPrev->GetSize().y : 52.0f * s;
        const float nextW = InNext ? InNext->GetSize().x : 56.0f * s;
        const float nextH = InNext ? InNext->GetSize().y : 52.0f * s;
        const float nameH =
            InLabel ? MeasurePadded(InLabel->GetText().empty() ? "YBOT" : InLabel->GetText(), InLabel->GetFontScale()).y
                    : prevH;
        place(InNext, FAnchors::BottomRight(), BoxBR(inset, inset, nextW, nextH));
        place(InLabel, FAnchors::BottomRight(),
              BoxBR(inset + nextW + gap, inset + (prevH - nameH) * 0.5f, nameW, nameH));
        place(InPrev, FAnchors::BottomRight(), BoxBR(inset + nextW + gap + nameW + gap, inset, prevW, prevH));
    }

} // namespace Leon
