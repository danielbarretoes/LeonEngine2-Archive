#pragma once

#include "Gameplay/UActorComponent.hpp"
#include "Engine/ECollisionChannel.hpp"

#include <glm/glm.hpp>

namespace Leon {

    /**
     * Maintains a desired camera arm length and retracts when the probe hits world geometry.
     */
    class USpringArmComponent : public UActorComponent {
    public:
        USpringArmComponent(const std::string& InName = "SpringArmComponent");

        void Tick(float DeltaSeconds) override;

        void UpdateDesiredArmLocation(const glm::vec3& InOrigin, const glm::vec3& InForward, const glm::vec3& InRight,
                                      const glm::vec3& InUp);

        glm::vec3 GetTargetLocation() const { return TargetLocation; }
        glm::vec3 GetDesiredLocation() const { return DesiredLocation; }
        float GetCurrentArmLength() const { return CurrentArmLength; }

        float TargetArmLength = 3.4f;
        glm::vec3 SocketOffset{0.45f, 0.25f, 0.0f};
        glm::vec3 TargetOffset{0.0f};
        float ProbeSize = 0.18f;
        ECollisionChannel ProbeChannel = ECollisionChannel::Camera;
        bool bDoCollisionTest = true;
        float CameraLagSpeed = 0.0f;

    private:
        glm::vec3 DesiredLocation{0.0f};
        glm::vec3 TargetLocation{0.0f};
        float CurrentArmLength = 3.4f;
        bool bHasHit = false;
        glm::vec3 LastHitLocation{0.0f};
        glm::vec3 LastHitNormal{0.0f, 1.0f, 0.0f};
    };

} // namespace Leon
