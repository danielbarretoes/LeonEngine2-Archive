#pragma once

#include "UMG/FUILayout.hpp"
#include "FLeonTournamentUITheme.hpp"
#include "Core/FInput.hpp"

namespace Leon {

    class ULeonTournamentGameInstance;
    class ALeonTournamentGameMode;
    class ALeonTournamentGameState;
    class APlayerController;
    class UWidget;
    class UImage;
    class UTextBlock;
    class UButton;
    class UCanvasPanel;
    class UTableView;

    /** Product accessors on top of engine FUILayout. */
    struct FLeonTournamentUILayout : FUILayout {
        static ULeonTournamentGameInstance* GI();
        static bool GamepadEdge(int InButton, bool& InOutWasDown) { return FUILayout::GamepadEdge(InButton, InOutWasDown); }
        static ALeonTournamentGameMode* GM(APlayerController* InPC);
        static ALeonTournamentGameState* GS(APlayerController* InPC);
        static bool IsClientWorld(APlayerController* InPC);
        static glm::vec2 ResolveViewportSize(const UCanvasPanel* InRoot) {
            return FUILayout::ResolveViewportSize(InRoot);
        }
        static float SyncResolutionScale(UCanvasPanel& InRoot, float& InOutAppliedScale, glm::vec2& InOutAppliedViewport,
                                         float InDesignContentHeight = 0.0f, float InVerticalMargin = 96.0f) {
            return FUILayout::SyncResolutionScale(InRoot, InOutAppliedScale, InOutAppliedViewport, InDesignContentHeight,
                                                  InVerticalMargin);
        }

        static TRef<UButton> MakeThemedButton(const std::string& InName, const std::string& InLabel, float InFont,
                                              float InMinW = 200.0f, float InMinH = 0.0f);
        static TRef<UImage> MakePanel(const std::string& InName, const glm::vec4& InTint = FLeonTournamentUITheme::PanelBg);
        static TRef<UImage> MakeAccentStrip(const std::string& InName, bool bVertical = true);
        static void StyleTitle(UTextBlock& InText);
        static void StyleHero(UTextBlock& InText);
        static void StyleCaption(UTextBlock& InText);
        static void StyleBody(UTextBlock& InText);
        static void StyleAccent(UTextBlock& InText);
        static void ApplyThemedButtonColors(UButton& InButton, bool bSelected = false);
        static void ApplyScoreboardTableTheme(UTableView& InTable);

        static void ApplyMenuRailLayout(UCanvasPanel& InRoot, const TRef<UWidget>& InPanel, const TRef<UButton>& InPrev,
                                        const TRef<UTextBlock>& InLabel, const TRef<UButton>& InNext, bool bLobby,
                                        float InScale = 0.0f);
        static void ApplyModalLayout(UCanvasPanel& InRoot, const TRef<UImage>& InDim, const TRef<UImage>& InPanel,
                                   const TRef<UImage>& InAccent, float InPanelW, float InPanelH);
    };

} // namespace Leon
