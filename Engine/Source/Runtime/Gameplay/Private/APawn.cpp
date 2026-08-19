#include "Gameplay/APawn.hpp"
#include "Core/FLog.hpp"
#include "Gameplay/AController.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Gameplay/APlayerState.hpp"
#include "Engine/UWorld.hpp"

namespace Leon {

    APawn::APawn(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AActor(InHandle, InWorld, InName) {
        SetClass("APawn");
    }

    APlayerState* APawn::GetPlayerState() const {
        if (Controller && Controller->GetPlayerState())
            return Controller->GetPlayerState();
        return PlayerState;
    }

    bool APawn::IsLocallyControlled() const {
        if (!Controller)
            return false;
        if (!World)
            return true;
        const ENetMode mode = World->GetNetMode();
        if (mode == ENetMode::Client)
            return GetLocalRole() == ENetRole::AutonomousProxy;
        return Controller == World->GetFirstPlayerController();
    }

    void APawn::PossessedBy(AController* InController) {
        Controller = InController;
        if (InController)
            PlayerState = InController->GetPlayerState();
        LE_CORE_INFO("APawn '{0}' possessed", GetName());
    }

    void APawn::UnPossessed() {
        LE_CORE_INFO("APawn '{0}' unpossessed", GetName());
        Controller = nullptr;
        PlayerState = nullptr;
    }

} // namespace Leon
