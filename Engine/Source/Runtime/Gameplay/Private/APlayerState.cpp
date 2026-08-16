#include "Gameplay/APlayerState.hpp"

namespace Leon {

    APlayerState::APlayerState(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AActor(InHandle, InWorld, InName) {
        SetClass("APlayerState");
    }

} // namespace Leon
