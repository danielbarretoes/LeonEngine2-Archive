#pragma once

#include "Gameplay/UNavMovementComponent.hpp"
#include "Gameplay/EMovementMode.hpp"

#include <algorithm>
#include <cstdint>
#include <glm/glm.hpp>

namespace Leon {

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
        float GetMaxWalkSpeedCrouched() const { return MaxWalkSpeedCrouched; }
        void SetMaxWalkSpeedCrouched(float InSpeed) { MaxWalkSpeedCrouched = InSpeed; }
        float GetJumpZVelocity() const { return JumpZVelocity; }
        void SetJumpZVelocity(float InZ) { JumpZVelocity = InZ; }
        float GetGravityScale() const { return GravityScale; }
        void SetGravityScale(float InScale) { GravityScale = InScale; }
        float GetGravityZ() const { return GravityZ; }
        void SetGravityZ(float InZ) { GravityZ = InZ; }
        float GetAirControl() const { return AirControl; }
        void SetAirControl(float InControl) { AirControl = InControl; }
        int32_t GetJumpMaxCount() const { return JumpMaxCount; }
        void SetJumpMaxCount(int32_t InCount) { JumpMaxCount = std::max(1, InCount); }
        void AddImpulse(const glm::vec3& InImpulse);

        bool FindFloor(float InSweepDistance, struct FHitResult& OutHit) const;
        /** Accumulate frame Δt and run fixed-step PerformMovement (same clock as physics). */
        void TickMovement(float InDeltaSeconds);
        void PerformMovement(float DeltaSeconds);
        float GetMaxStepHeight() const { return MaxStepHeight; }
        void SetMaxStepHeight(float InHeight) { MaxStepHeight = std::max(0.0f, InHeight); }
        void SmoothClientPosition(float DeltaSeconds);
        void StopMovementImmediately();
        void ResetForRespawn();

        /**
         * Push the capsule out of WorldStatic overlaps along the SAT MTD.
         * Walkable floors (Normal.y > 0.7) are skipped so SnapToFloor stays in charge.
         * No-op on SimulatedProxy (pose comes from net).
         */
        bool ResolvePenetration();

        void SetFloorZ(float InZ) { FloorZ = InZ; }
        float GetFloorZ() const { return FloorZ; }

    private:
        void ApplyGravity(float DeltaSeconds);
        void MoveAlongFloor(float DeltaSeconds);
        void MoveThroughAir(float DeltaSeconds);
        void TryStepUp(class ACharacter* InCharacter, const glm::vec3& InDelta);
        class ACharacter* GetCharacter() const;

        EMovementMode MovementMode = EMovementMode::Walking;
        glm::vec3 Velocity{0.0f};
        glm::vec3 Acceleration{0.0f};
        glm::vec3 PendingInputVector{0.0f};
        float MaxWalkSpeed = 7.5f;
        float MaxWalkSpeedCrouched = 3.5f;
        float MaxAcceleration = 28.0f;
        float BrakingDecelerationWalking = 24.0f;
        float GroundFriction = 8.0f;
        float JumpZVelocity = 9.0f;
        float GravityScale = 1.0f;
        float GravityZ = 22.0f;
        float AirControl = 0.45f;
        float FloorZ = 0.0f;
        float MaxStepHeight = 0.4f;
        int32_t JumpMaxCount = 2;
        int32_t JumpCurrentCount = 0;
        bool bPressedJump = false;
        bool bWasFalling = false;
        float MovementAccumulator = 0.0f;
    };

} // namespace Leon
