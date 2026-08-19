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
        static bool GamepadEdge(int InButton, bool& InOutWasDown);
        static ALeonTournamentGameMode* GM(APlayerController* InPC);
        static ALeonTournamentGameState* GS(APlayerController* InPC);
        static bool IsClientWorld(APlayerController* InPC);
        static glm::vec2 ResolveViewportSize(const UCanvasPanel* InRoot);
        /** Scale root slots/fonts from last AppliedScale to the current window LayoutScale. */
        static float SyncResolutionScale(UCanvasPanel& InRoot, float& InOutAppliedScale, glm::vec2& InOutAppliedViewport);
        static void ApplyMenuRailLayout(UCanvasPanel& InRoot, const TRef<UWidget>& InPanel, const TRef<UButton>& InPrev,
                                        const TRef<UTextBlock>& InLabel, const TRef<UButton>& InNext, bool bLobby);
    };

} // namespace Leon
