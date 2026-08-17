#pragma once

#include "Core/FInput.hpp"
#include "Core/FInputSettings.hpp"
#include "Gameplay/APawn.hpp"
#include "Gameplay/USkeletalMeshComponent.hpp"
#include "Gameplay/UPrimitiveComponent.hpp"
#include "Gameplay/UCharacterMovementComponent.hpp"
#include "Gameplay/USpringArmComponent.hpp"
#include "Gameplay/EMovementMode.hpp"
#include "Assets/FAnimTypes.hpp"
#include "Physics/FHitResult.hpp"

#include <algorithm>

namespace Leon {

    class UAnimInstance;
    class APhysicsVolume;

    /**
     * Ground character: CapsuleComponent (RootComponent) + CharacterMovement + Mesh.
     * Actor location is the capsule center (Unreal convention). Mesh attaches under the capsule.
     */
    class ACharacter : public APawn {
    public:
        ACharacter() = default;
        ACharacter(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "Character");
        ~ACharacter() override = default;

        void PostInitializeComponents() override;
        void Tick(float DeltaSeconds) override;
        void SetupPlayerInputComponent(float DeltaSeconds) override;

        TRef<UCapsuleComponent> GetCapsuleComponent() const { return CapsuleComponent; }
        TRef<UCharacterMovementComponent> GetCharacterMovement() const { return CharacterMovement; }
        TRef<USkeletalMeshComponent> GetMesh() const { return Mesh; }
        TRef<USpringArmComponent> GetSpringArm() const { return SpringArm; }

        float GetMoveSpeed() const { return CharacterMovement ? CharacterMovement->GetMaxWalkSpeed() : 6.0f; }
        void SetMoveSpeed(float InSpeed) {
            if (CharacterMovement)
                CharacterMovement->SetMaxWalkSpeed(InSpeed);
        }

        float GetFloorZ() const { return FloorZ; }
        void SetFloorZ(float InZ) {
            FloorZ = InZ;
            if (CharacterMovement)
                CharacterMovement->SetFloorZ(InZ);
        }

        float GetEyeHeight() const { return EyeHeight; }
        void SetEyeHeight(float InHeight) { EyeHeight = InHeight; }

        float GetCapsuleRadius() const { return CapsuleRadius; }
        void SetCapsuleRadius(float InRadius) { CapsuleRadius = InRadius; }
        float GetCapsuleHalfHeight() const { return GetCapsuleHeight() * 0.5f; }

        bool IsThirdPerson() const { return bThirdPerson; }
        void SetThirdPerson(bool bEnabled);

        const FAnimRepState& GetAnimRepState() const { return AnimRepState; }
        void SetAnimRepState(const FAnimRepState& InState) { AnimRepState = InState; }

        virtual void UpdateAnimInstance(UAnimInstance& InAnim) const;

        virtual void Jump();
        virtual void StopJumping();
        virtual bool CanJump() const;
        virtual void Landed(const FHitResult& InHit);
        virtual void OnMovementModeChanged(EMovementMode InPrevMode, EMovementMode InNewMode);

        bool IsFalling() const;
        bool IsMovingOnGround() const;
        float GetVerticalVelocity() const;

        glm::vec3 MoveBlocked(const glm::vec3& InWorldDelta);
        void GetCapsuleAABB(glm::vec3& OutMin, glm::vec3& OutMax) const;
        /** Full capsule height (diameter along up axis). */
        /** Full capsule height (diameter along up axis). Tuned near human mesh (~1.8 m for EyeHeight 1.7). */
        float GetCapsuleHeight() const { return EyeHeight + 0.1f; }
        void SnapToFloorPublic() { SnapToFloor(); }

        /** World-space eye / first-person view location (Unreal GetPawnViewLocation lite). */
        glm::vec3 GetPawnViewLocation() const;

        /** Camera used for rendering / crosshair (first person eye, or spring-arm in third person). */
        void GetViewPoint(glm::vec3& OutLocation, glm::vec3& OutForward) const;

        APhysicsVolume* GetPhysicsVolume() const { return PhysicsVolume; }

        float GetControlYaw() const;
        float GetControlPitch() const;
        glm::vec3 GetControlRotation() const { return {GetControlPitch(), GetControlYaw(), 0.0f}; }
        void SetControlYaw(float InYaw);
        void SetControlPitch(float InPitch);
        void SetControlRotation(const glm::vec3& InRotation);

        /** Planar engine-forward from control yaw (Z=0). */
        glm::vec3 GetControlPlanarForward() const;
        glm::vec3 GetControlLookDirection() const;

        float GetSprintMultiplier() const { return SprintMultiplier; }
        float GetLookSensitivity() const { return LookSensitivity; }

        void ApplyLookInput(float DeltaSeconds, bool bRequireHeldButton);
        void ApplyMoveInput(float DeltaSeconds, float InSpeedScale);
        void ApplyYawOnlyActorRotation();
        void AddMovementInput(const glm::vec3& InWorldDirection, float InScale = 1.0f);

        /** Degrees in [-180, 180]: 0 forward, +90 right, 180 back, -90 left. */
        static float ComputeLocomotionDirection(const glm::vec3& InPlanarMove, const glm::vec3& InPlanarForward);

        void ResetMovementForRespawn();
        void SetMeshHiddenInGame(bool bHidden);
        bool IsMeshHiddenInGame() const { return bMeshHiddenInGame; }

    protected:
        /** When false, look still drives the camera but does not yaw the pawn (death free-cam). */
        virtual bool ShouldApplyControlYawToActor() const { return true; }

    private:
        void SnapToFloor();
        void UpdateCameraFromView();
        void UpdateAnimFromMovement(float DeltaSeconds);
        void UpdatePhysicsVolume();
        void UpdateMeshVisibility();
        void DrawCharacterDebug() const;

        TRef<UCapsuleComponent> CapsuleComponent;
        TRef<UCharacterMovementComponent> CharacterMovement;
        TRef<USkeletalMeshComponent> Mesh;
        TRef<USpringArmComponent> SpringArm;
        FAnimRepState AnimRepState;
        APhysicsVolume* PhysicsVolume = nullptr;

        float SprintMultiplier = 1.8f;
        float LookSensitivity = 0.12f;
        float FloorZ = 0.0f;
        float CapsuleRadius = 0.4f;
        float EyeHeight = 1.7f;
        bool bThirdPerson = false;
        bool bMeshHiddenInGame = false;
        bool bJumpWasDown = false;

        float Yaw = -90.0f;
        float Pitch = 0.0f;
        glm::vec2 LastMousePos{0.0f, 0.0f};
        bool bFirstMouse = true;
        glm::vec3 LastLocation{0.0f};
        glm::vec3 LastMoveDir{0.0f, 0.0f, 1.0f};
        bool bHasLastLocation = false;
    };

} // namespace Leon
