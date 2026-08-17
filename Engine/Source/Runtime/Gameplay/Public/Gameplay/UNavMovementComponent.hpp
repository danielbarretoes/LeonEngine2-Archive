#pragma once

#include "Gameplay/UActorComponent.hpp"

#include <glm/glm.hpp>

namespace Leon {

    class APawn;

    /**
     * Base movement component. RequestDirectMove is the AI/nav entry point.
     */
    class UNavMovementComponent : public UActorComponent {
    public:
        UNavMovementComponent(const std::string& InName = "NavMovementComponent");

        virtual void RequestDirectMove(const glm::vec3& InMoveVelocity, bool bForceMaxSpeed);
        virtual void StopActiveMovement();
        virtual void StopMovementKeepPathing() { StopActiveMovement(); }

        bool HasRequestedVelocity() const { return bHasRequestedVelocity; }
        const glm::vec3& GetRequestedVelocity() const { return RequestedVelocity; }

        APawn* GetPawnOwner() const;

    protected:
        glm::vec3 RequestedVelocity{0.0f};
        bool bHasRequestedVelocity = false;
    };

} // namespace Leon
