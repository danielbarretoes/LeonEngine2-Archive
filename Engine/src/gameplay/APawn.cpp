#include "gameplay/APawn.hpp"
#include "core/Log.hpp"

namespace Leon {

    APawn::APawn(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AActor(InHandle, InWorld, InName) {}

    void APawn::PossessedBy(APlayerController* InController) {
        m_Controller = InController;
        LE_CORE_INFO("APawn '{0}' possessed by PlayerController", GetName());
    }

    void APawn::UnPossessed() {
        LE_CORE_INFO("APawn '{0}' unpossessed", GetName());
        m_Controller = nullptr;
    }

} // namespace Leon
