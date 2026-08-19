#pragma once

#include "UMG/FUILayout.hpp"
#include "Core/FInput.hpp"

namespace Leon {

    class ULeonTournamentGameInstance;
    class ALeonTournamentGameMode;
    class ALeonTournamentGameState;
    class APlayerController;
    class UWidget;

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
        static void ApplyMenuRailLayout(UCanvasPanel& InRoot, const TRef<UWidget>& InPanel, const TRef<UButton>& InPrev,
                                        const TRef<UTextBlock>& InLabel, const TRef<UButton>& InNext, bool bLobby,
                                        float InScale = 0.0f);
    };

} // namespace Leon
