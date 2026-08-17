#include "Gameplay/UCharacterMovementComponent.hpp"
#include "Gameplay/ACharacter.hpp"
#include "Gameplay/APawn.hpp"
#include "Gameplay/ABlockingVolume.hpp"
#include "Engine/UWorld.hpp"
#include "Physics/FHitResult.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

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

    UCharacterMovementComponent::UCharacterMovementComponent(const std::string& InName)
        : UNavMovementComponent(InName) {}

    ACharacter* UCharacterMovementComponent::GetCharacter() const {
        return Owner ? dynamic_cast<ACharacter*>(Owner) : nullptr;
    }

    void UCharacterMovementComponent::BeginPlay() {
        auto* character = GetCharacter();
        if (character)
            FloorZ = character->GetFloorZ();
    }

    void UCharacterMovementComponent::AddInputVector(const glm::vec3& InWorldAccel) { PendingInputVector += InWorldAccel; }

    void UCharacterMovementComponent::ConsumeInputVector() { PendingInputVector = glm::vec3(0.0f); }

    void UCharacterMovementComponent::SetMovementMode(EMovementMode InMode) {
        if (MovementMode == InMode)
            return;
        const EMovementMode prev = MovementMode;
        MovementMode = InMode;
        if (auto* character = GetCharacter())
            character->OnMovementModeChanged(prev, InMode);
        if (prev == EMovementMode::Falling && (InMode == EMovementMode::Walking || InMode == EMovementMode::NavWalking)) {
            if (auto* character = GetCharacter()) {
                FHitResult land;
                land.bBlockingHit = true;
                land.Location = character->GetActorLocation();
                land.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
                land.ImpactPoint = land.Location;
                land.ImpactNormal = land.Normal;
                character->Landed(land);
            }
        }
    }

    bool UCharacterMovementComponent::CanJump() const { return IsMovingOnGround(); }

    void UCharacterMovementComponent::Jump() { bPressedJump = true; }

    void UCharacterMovementComponent::StopJumping() { bPressedJump = false; }

    bool UCharacterMovementComponent::DoJump() {
        if (!CanJump())
            return false;
        Velocity.y = JumpZVelocity;
        SetMovementMode(EMovementMode::Falling);
        bPressedJump = false;
        return true;
    }

    bool UCharacterMovementComponent::FindFloor(float InSweepDistance, FHitResult& OutHit) const {
        auto* character = GetCharacter();
        if (!character || !character->GetWorld())
            return false;
        glm::vec3 minB, maxB;
        character->GetCapsuleAABB(minB, maxB);
        glm::vec3 center = (minB + maxB) * 0.5f;
        glm::vec3 half = (maxB - minB) * 0.5f;
        center.y -= InSweepDistance * 0.5f;
        half.y += InSweepDistance * 0.5f;
        std::vector<FHitResult> hits;
        if (character->GetWorld()->OverlapMultiByChannel(center, half, ECollisionChannel::WorldStatic, character,
                                                         hits) <= 0)
            return false;
        OutHit = hits.front();
        return true;
    }

    bool UCharacterMovementComponent::ResolvePenetration() { return false; }

    void UCharacterMovementComponent::ApplyGravity(float DeltaSeconds) {
        float scale = GravityScale;
        if (auto* character = GetCharacter()) {
            if (APhysicsVolume* volume = character->GetPhysicsVolume())
                scale *= volume->GravityScale;
        }
        Velocity.y -= GravityZ * scale * DeltaSeconds;
    }

    void UCharacterMovementComponent::MoveAlongFloor(float DeltaSeconds) {
        auto* character = GetCharacter();
        if (!character)
            return;

        glm::vec3 wish = PendingInputVector;
        if (bHasRequestedVelocity) {
            if (glm::length(wish) > 1e-4f)
                wish += RequestedVelocity;
            else
                wish = RequestedVelocity;
            if (glm::length(wish) > MaxWalkSpeed)
                wish = glm::normalize(wish) * MaxWalkSpeed;
        }

        wish.y = 0.0f;
        Acceleration = wish;
        if (glm::length(wish) > 1e-4f) {
            glm::vec3 dir = glm::normalize(wish);
            float target = std::min(glm::length(wish), MaxWalkSpeed);
            glm::vec3 planar(Velocity.x, 0.0f, Velocity.z);
            planar += dir * MaxAcceleration * DeltaSeconds;
            if (glm::length(planar) > target)
                planar = glm::normalize(planar) * target;
            Velocity.x = planar.x;
            Velocity.z = planar.z;
        } else {
            glm::vec3 planar(Velocity.x, 0.0f, Velocity.z);
            float speed = glm::length(planar);
            float drop = std::max(BrakingDecelerationWalking, speed * GroundFriction) * DeltaSeconds;
            if (speed <= drop)
                planar = glm::vec3(0.0f);
            else
                planar *= (speed - drop) / speed;
            Velocity.x = planar.x;
            Velocity.z = planar.z;
            Acceleration = glm::vec3(0.0f);
        }

        glm::vec3 delta(Velocity.x * DeltaSeconds, 0.0f, Velocity.z * DeltaSeconds);
        character->MoveBlocked(delta);
        // Walking clamps to FloorZ / supporting geometry (engine spawn convention).
        character->SnapToFloorPublic();
        Velocity.y = 0.0f;

        FHitResult floorHit;
        const bool bGeoFloor = FindFloor(0.2f, floorHit);
        const float groundedY = FloorZ + character->GetEyeHeight();
        if (!bGeoFloor && character->GetActorLocation().y > groundedY + 0.25f)
            SetMovementMode(EMovementMode::Falling);
    }

    void UCharacterMovementComponent::MoveThroughAir(float DeltaSeconds) {
        auto* character = GetCharacter();
        if (!character)
            return;

        glm::vec3 wish = PendingInputVector;
        if (bHasRequestedVelocity)
            wish = RequestedVelocity;
        wish.y = 0.0f;
        if (glm::length(wish) > 1e-4f) {
            glm::vec3 dir = glm::normalize(wish);
            Velocity.x += dir.x * MaxAcceleration * AirControl * DeltaSeconds;
            Velocity.z += dir.z * MaxAcceleration * AirControl * DeltaSeconds;
            glm::vec3 planar(Velocity.x, 0.0f, Velocity.z);
            if (glm::length(planar) > MaxWalkSpeed)
                planar = glm::normalize(planar) * MaxWalkSpeed;
            Velocity.x = planar.x;
            Velocity.z = planar.z;
        }

        ApplyGravity(DeltaSeconds);
        glm::vec3 delta(Velocity.x * DeltaSeconds, Velocity.y * DeltaSeconds, Velocity.z * DeltaSeconds);
        glm::vec3 planar(delta.x, 0.0f, delta.z);
        if (glm::length(planar) > 1e-8f)
            character->MoveBlocked(planar);

        auto& t = character->GetTransform();
        t.Translation.y += delta.y;

        FHitResult floorHit;
        const bool bFloor = FindFloor(0.15f, floorHit);
        const float groundedY = FloorZ + character->GetEyeHeight();
        if (Velocity.y <= 0.0f && (bFloor || character->GetActorLocation().y <= groundedY + 0.02f)) {
            character->SnapToFloorPublic();
            Velocity.y = 0.0f;
            SetMovementMode(EMovementMode::Walking);
        }
    }

    void UCharacterMovementComponent::PerformMovement(float DeltaSeconds) {
        if (DeltaSeconds <= 0.0f)
            return;
        if (bPressedJump)
            DoJump();

        if (IsMovingOnGround())
            MoveAlongFloor(DeltaSeconds);
        else if (IsFalling())
            MoveThroughAir(DeltaSeconds);

        ConsumeInputVector();
        bHasRequestedVelocity = false;
        RequestedVelocity = glm::vec3(0.0f);
        SmoothClientPosition(DeltaSeconds);
    }

    void UCharacterMovementComponent::SmoothClientPosition(float DeltaSeconds) {
        // Extension point for client correction / smoothing. Authority currently simulates fully.
        (void)DeltaSeconds;
    }

    void UCharacterMovementComponent::Tick(float DeltaSeconds) {
        // ACharacter drives PerformMovement after input so camera/anim see this frame's pose.
        if (GetCharacter())
            return;
        auto* pawn = GetPawnOwner();
        if (!pawn)
            return;
        if (!pawn->HasAuthority() && pawn->GetLocalRole() == ENetRole::SimulatedProxy)
            return;
        PerformMovement(DeltaSeconds);
        (void)DeltaSeconds;
    }

} // namespace Leon
