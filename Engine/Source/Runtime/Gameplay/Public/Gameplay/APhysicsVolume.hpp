#pragma once

#include "Gameplay/AActor.hpp"
#include "Gameplay/UPrimitiveComponent.hpp"

namespace Leon {

    /**
     * Volume that can override gravity / fluid properties for overlapping characters.
     */
    class APhysicsVolume : public AActor {
    public:
        APhysicsVolume() = default;
        APhysicsVolume(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "PhysicsVolume");

        void PostInitializeComponents() override;

        float GravityScale = 1.0f;
        float TerminalVelocity = 40.0f;
        float FluidFriction = 0.3f;
        int32_t Priority = 0;
        bool bWaterVolume = false;

        TRef<UBoxComponent> GetBoxComponent() const { return Box; }
        bool EncompassesPoint(const glm::vec3& InPoint) const;

    private:
        TRef<UBoxComponent> Box;
    };

} // namespace Leon
