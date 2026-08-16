#include "Gameplay/APlayerStart.hpp"

namespace Leon {

    APlayerStart::APlayerStart(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AActor(InHandle, InWorld, InName) {
        SetClass("APlayerStart");
    }

} // namespace Leon
