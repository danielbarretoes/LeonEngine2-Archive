#include "Gameplay/UNavMovementComponent.hpp"
#include "Gameplay/APawn.hpp"

namespace Leon {

    UNavMovementComponent::UNavMovementComponent(const std::string& InName) : UActorComponent(InName) {}

    APawn* UNavMovementComponent::GetPawnOwner() const { return Owner ? dynamic_cast<APawn*>(Owner) : nullptr; }

    void UNavMovementComponent::RequestDirectMove(const glm::vec3& InMoveVelocity, bool bForceMaxSpeed) {
        (void)bForceMaxSpeed;
        RequestedVelocity = InMoveVelocity;
        bHasRequestedVelocity = glm::length(InMoveVelocity) > 1e-5f;
    }

    void UNavMovementComponent::StopActiveMovement() {
        RequestedVelocity = glm::vec3(0.0f);
        bHasRequestedVelocity = false;
    }

} // namespace Leon
