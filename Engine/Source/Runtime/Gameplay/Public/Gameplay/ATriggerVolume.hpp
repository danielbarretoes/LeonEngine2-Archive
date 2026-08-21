#pragma once

#include "Gameplay/AActor.hpp"
#include "Gameplay/UPrimitiveComponent.hpp"

namespace Leon {

    /**
     * Box volume that generates overlaps and does not block movement.
     */
    class ATriggerVolume : public AActor {
    public:
        ATriggerVolume() = default;
        ATriggerVolume(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "TriggerVolume");

        void PostInitializeComponents() override;

        void SetEnabled(bool bInEnabled);
        [[nodiscard]] bool IsEnabled() const { return bEnabled; }

        TRef<UBoxComponent> GetBoxComponent() const { return Box; }

    private:
        void ApplyCollisionState();

        TRef<UBoxComponent> Box;
        bool bEnabled = true;
    };

} // namespace Leon
