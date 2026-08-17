#pragma once

#include "Gameplay/UActorComponent.hpp"
#include "Gameplay/EMovementMode.hpp"

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

    class UCharacterMovementComponent : public UNavMovementComponent {
    public:
        UCharacterMovementComponent(const std::string& InName = "CharacterMovement");

        void BeginPlay() override;
        void Tick(float DeltaSeconds) override;

        void AddInputVector(const glm::vec3& InWorldAccel);
        void ConsumeInputVector();
        glm::vec3 GetPendingInputVector() const { return PendingInputVector; }

        void Jump();
        void StopJumping();
        bool CanJump() const;
        bool DoJump();

        bool IsFalling() const { return MovementMode == EMovementMode::Falling; }
        bool IsMovingOnGround() const {
            return MovementMode == EMovementMode::Walking || MovementMode == EMovementMode::NavWalking;
        }

        EMovementMode GetMovementMode() const { return MovementMode; }
        void SetMovementMode(EMovementMode InMode);

        const glm::vec3& GetVelocity() const { return Velocity; }
        void SetVelocity(const glm::vec3& InVelocity) { Velocity = InVelocity; }
        const glm::vec3& GetAcceleration() const { return Acceleration; }

        float GetMaxWalkSpeed() const { return MaxWalkSpeed; }
        void SetMaxWalkSpeed(float InSpeed) { MaxWalkSpeed = InSpeed; }
        float GetJumpZVelocity() const { return JumpZVelocity; }
        void SetJumpZVelocity(float InZ) { JumpZVelocity = InZ; }
        float GetGravityScale() const { return GravityScale; }
        void SetGravityScale(float InScale) { GravityScale = InScale; }
        float GetGravityZ() const { return GravityZ; }
        void SetGravityZ(float InZ) { GravityZ = InZ; }
        float GetAirControl() const { return AirControl; }
        void SetAirControl(float InControl) { AirControl = InControl; }

        bool FindFloor(float InSweepDistance, struct FHitResult& OutHit) const;
        void PerformMovement(float DeltaSeconds);
        void SmoothClientPosition(float DeltaSeconds);

        void SetFloorZ(float InZ) { FloorZ = InZ; }
        float GetFloorZ() const { return FloorZ; }

    private:
        void ApplyGravity(float DeltaSeconds);
        void MoveAlongFloor(float DeltaSeconds);
        void MoveThroughAir(float DeltaSeconds);
        bool ResolvePenetration();
        class ACharacter* GetCharacter() const;

        EMovementMode MovementMode = EMovementMode::Walking;
        glm::vec3 Velocity{0.0f};
        glm::vec3 Acceleration{0.0f};
        glm::vec3 PendingInputVector{0.0f};
        float MaxWalkSpeed = 6.0f;
        float MaxAcceleration = 24.0f;
        float BrakingDecelerationWalking = 24.0f;
        float GroundFriction = 8.0f;
        float JumpZVelocity = 8.0f;
        float GravityScale = 1.0f;
        float GravityZ = 22.0f;
        float AirControl = 0.25f;
        float FloorZ = 0.0f;
        float MaxStepHeight = 0.4f;
        bool bPressedJump = false;
        bool bWasFalling = false;
    };

} // namespace Leon
