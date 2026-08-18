#pragma once

#include "UMG/FUILayout.hpp"
#include "Core/FInput.hpp"

namespace Leon {

    class ULeonTournamentGameInstance;
    class ALeonTournamentGameMode;
    class ALeonTournamentGameState;
    class APlayerController;

    /** Product accessors on top of engine FUILayout. */
    struct FLeonTournamentUILayout : FUILayout {
        static ULeonTournamentGameInstance* GI();
        static bool GamepadEdge(int InButton, bool& InOutWasDown);
        static ALeonTournamentGameMode* GM(APlayerController* InPC);
        static ALeonTournamentGameState* GS(APlayerController* InPC);
        static bool IsClientWorld(APlayerController* InPC);
    };

} // namespace Leon
