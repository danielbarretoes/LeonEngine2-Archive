#include "FLeonTournamentUILayout.hpp"
#include "ULeonTournamentGameInstance.hpp"
#include "ALeonTournamentGameMode.hpp"
#include "ALeonTournamentGameState.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Engine/UEngine.hpp"
#include "Engine/UWorld.hpp"
#include "Core/FInputSettings.hpp"

namespace Leon {

    ULeonTournamentGameInstance* FLeonTournamentUILayout::GI() {
        return UEngine::HasInstance()
                   ? dynamic_cast<ULeonTournamentGameInstance*>(UEngine::Get().GetGameInstance().get())
                   : nullptr;
    }

    bool FLeonTournamentUILayout::GamepadEdge(int InButton, bool& InOutWasDown) {
        const FInputSettings& input = FInputSettings::Get();
        if (!input.bEnableGamepad || !FInput::IsGamepadConnected(input.GamepadId)) {
            InOutWasDown = false;
            return false;
        }
        const bool down = FInput::IsGamepadButtonPressed(InButton, input.GamepadId);
        const bool edge = down && !InOutWasDown;
        InOutWasDown = down;
        return edge;
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

} // namespace Leon
