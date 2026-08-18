#pragma once

#include "Gameplay/APickup.hpp"

#include <glm/glm.hpp>

namespace Leon {

    /** Showcase collectible — engine APickup with a spinning visual and PrintString give. */
    class ASandboxDemoPickup : public APickup {
    public:
        ASandboxDemoPickup() = default;
        ASandboxDemoPickup(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "SandboxDemoPickup");

        void SetPickupActive(bool bInActive) override;

    protected:
        bool GiveTo(APawn* InPawn) override;
        void BuildVisual() override;

        glm::vec3 VisualColor{0.25f, 0.78f, 1.0f};
    };

} // namespace Leon
