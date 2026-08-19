#include "Gameplay/APlayerState.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Engine/UWorld.hpp"

namespace Leon {

    APlayerState::APlayerState(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AActor(InHandle, InWorld, InName) {
        SetClass("APlayerState");
    }

    APlayerController* APlayerState::GetPlayerController() const {
        if (!World)
            return nullptr;
        for (APlayerController* pc : World->GetPlayerControllers()) {
            if (pc && pc->GetPlayerState() == this)
                return pc;
        }
        return nullptr;
    }

} // namespace Leon
