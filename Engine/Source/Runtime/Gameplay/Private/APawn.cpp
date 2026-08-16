#include "Gameplay/APawn.hpp"
#include "Core/FLog.hpp"

namespace Leon {

    APawn::APawn(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AActor(InHandle, InWorld, InName) {
        SetClass("APawn");
    }

    void APawn::PossessedBy(APlayerController* InController) {
        Controller = InController;
        LE_CORE_INFO("APawn '{0}' possessed by PlayerController", GetName());
    }

    void APawn::UnPossessed() {
        LE_CORE_INFO("APawn '{0}' unpossessed", GetName());
        Controller = nullptr;
    }

} // namespace Leon
