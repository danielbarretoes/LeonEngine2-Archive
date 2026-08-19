#include "Gameplay/UCharacterMovementComponent.hpp"
#include "Gameplay/ACharacter.hpp"
#include "Gameplay/APawn.hpp"
#include "Gameplay/APhysicsVolume.hpp"
#include "Engine/UWorld.hpp"
#include "Physics/FHitResult.hpp"
#include "Physics/IPhysicsScene.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Leon {

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

    void UCharacterMovementComponent::AddImpulse(const glm::vec3& InImpulse) {
        Velocity += InImpulse;
        if (InImpulse.y > 0.25f || !IsMovingOnGround())
            SetMovementMode(EMovementMode::Falling);
    }

    void UCharacterMovementComponent::SetMovementMode(EMovementMode InMode) {
        if (MovementMode == InMode)
            return;
        const EMovementMode prev = MovementMode;
        MovementMode = InMode;
        if (auto* character = GetCharacter())
            character->OnMovementModeChanged(prev, InMode);
        if (prev == EMovementMode::Falling && (InMode == EMovementMode::Walking || InMode == EMovementMode::NavWalking)) {
            JumpCurrentCount = 0;
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

    bool UCharacterMovementComponent::CanJump() const {
        if (IsMovingOnGround())
            return true;
        if (IsFalling() && JumpCurrentCount < JumpMaxCount)
            return true;
        return false;
    }

    void UCharacterMovementComponent::Jump() { bPressedJump = true; }

    void UCharacterMovementComponent::StopJumping() { bPressedJump = false; }

    bool UCharacterMovementComponent::DoJump() {
        if (!CanJump())
            return false;
        const float scale = (JumpCurrentCount == 0) ? 1.0f : 0.88f;
        Velocity.y = JumpZVelocity * scale;
        ++JumpCurrentCount;
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

    bool UCharacterMovementComponent::ResolvePenetration() {
        auto* character = GetCharacter();
        if (!character || !character->GetWorld())
            return false;
        if (character->GetLocalRole() == ENetRole::SimulatedProxy)
            return false;

        // Flow: capsule depenetration
        // 1. Overlap WorldStatic with the capsule AABB
        // 2. Pick the deepest SAT axis (skip walkable floors — SnapToFloor owns those)
        // 3. Push along the MTD + skin; repeat a few times if still overlapping
        constexpr float kSkin = 0.02f;
        constexpr float kWalkableY = 0.7f;
        constexpr int kMaxIters = 4;
        bool bMoved = false;

        for (int iter = 0; iter < kMaxIters; ++iter) {
            glm::vec3 minB, maxB;
            character->GetCapsuleAABB(minB, maxB);
            const glm::vec3 center = (minB + maxB) * 0.5f;
            const glm::vec3 half = (maxB - minB) * 0.5f;
            std::vector<FHitResult> hits;
            if (character->GetWorld()->OverlapMultiByChannel(center, half, ECollisionChannel::WorldStatic, character,
                                                             hits) <= 0)
                break;

            const FHitResult* best = nullptr;
            float bestDepth = 0.0f;
            for (const auto& hit : hits) {
                if (!hit.bBlockingHit || hit.PenetrationDepth <= 1e-5f)
                    continue;
                if (hit.Normal.y > kWalkableY)
                    continue;
                if (hit.PenetrationDepth > bestDepth) {
                    bestDepth = hit.PenetrationDepth;
                    best = &hit;
                }
            }
            if (!best)
                break;

            glm::vec3 n = best->Normal;
            const float nLen = glm::length(n);
            if (nLen < 1e-6f)
                break;
            n /= nLen;
            character->SetActorLocation(character->GetActorLocation() + n * (bestDepth + kSkin));
            bMoved = true;
        }
        return bMoved;
    }

    void UCharacterMovementComponent::TryStepUp(ACharacter* InCharacter, const glm::vec3& InDelta) {
        if (!InCharacter || !InCharacter->GetWorld() || MaxStepHeight <= 1e-4f)
            return;
        UWorld* world = InCharacter->GetWorld();
        const glm::vec3 start = InCharacter->GetActorLocation();
        const float radius = InCharacter->GetCapsuleRadius();
        FHitResult upHit;
        float up = MaxStepHeight;
        const glm::vec3 upEnd = start + glm::vec3(0.0f, MaxStepHeight, 0.0f);
        if (world->SweepSingleByChannel(start, upEnd, radius, ECollisionChannel::WorldStatic, InCharacter, upHit) &&
            upHit.bBlockingHit) {
            if (upHit.Distance < 0.04f)
                return;
            up = std::max(upHit.Distance - 0.02f, 0.0f);
        }
        InCharacter->SetActorLocation(start + glm::vec3(0.0f, up, 0.0f));
        InCharacter->MoveBlocked(InDelta);
    }

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
        const glm::vec3 origin = character->GetActorLocation();
        character->MoveBlocked(delta);
        glm::vec3 planarMoved = character->GetActorLocation() - origin;
        planarMoved.y = 0.0f;
        if (glm::length(delta) > 0.02f && glm::length(planarMoved) < glm::length(delta) * 0.45f) {
            character->SetActorLocation(origin);
            TryStepUp(character, delta);
        }
        character->SnapToFloorPublic();
        Velocity.y = 0.0f;

        FHitResult floorHit;
        const bool bGeoFloor = FindFloor(0.2f, floorHit);
        const float groundedY = FloorZ + character->GetCapsuleHalfHeight();
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
        const float groundedY = FloorZ + character->GetCapsuleHalfHeight();
        if (Velocity.y <= 0.0f && (bFloor || character->GetActorLocation().y <= groundedY + 0.02f)) {
            character->SnapToFloorPublic();
            Velocity.y = 0.0f;
            SetMovementMode(EMovementMode::Walking);
        }
    }

    void UCharacterMovementComponent::StopMovementImmediately() {
        Velocity = glm::vec3(0.0f);
        Acceleration = glm::vec3(0.0f);
        PendingInputVector = glm::vec3(0.0f);
        RequestedVelocity = glm::vec3(0.0f);
        bHasRequestedVelocity = false;
        bPressedJump = false;
    }

    void UCharacterMovementComponent::ResetForRespawn() {
        StopMovementImmediately();
        JumpCurrentCount = 0;
        SetMovementMode(EMovementMode::Walking);
    }

    void UCharacterMovementComponent::TickMovement(float InDeltaSeconds) {
        const float clamped = std::min(std::max(InDeltaSeconds, 0.0f), kPhysicsMaxFrameDeltaSeconds);
        MovementAccumulator += clamped;
        // Keep the same wish vector across fixed substeps within one frame.
        const glm::vec3 savedInput = PendingInputVector;
        const glm::vec3 savedRequested = RequestedVelocity;
        const bool bSavedRequested = bHasRequestedVelocity;
        int32_t steps = 0;
        while (MovementAccumulator >= kPhysicsFixedDeltaSeconds && steps < kPhysicsMaxSubsteps) {
            PendingInputVector = savedInput;
            RequestedVelocity = savedRequested;
            bHasRequestedVelocity = bSavedRequested;
            PerformMovement(kPhysicsFixedDeltaSeconds);
            MovementAccumulator -= kPhysicsFixedDeltaSeconds;
            ++steps;
        }
        if (steps >= kPhysicsMaxSubsteps)
            MovementAccumulator = 0.0f;
        ConsumeInputVector();
        bHasRequestedVelocity = false;
        RequestedVelocity = glm::vec3(0.0f);
    }

    void UCharacterMovementComponent::PerformMovement(float DeltaSeconds) {
        if (DeltaSeconds <= 0.0f)
            return;
        auto* character = GetCharacter();
        if (character && character->GetLocalRole() == ENetRole::SimulatedProxy)
            return;
        if (character && character->IsRagdoll()) {
            ConsumeInputVector();
            bHasRequestedVelocity = false;
            RequestedVelocity = glm::vec3(0.0f);
            return;
        }
        if (MovementMode == EMovementMode::None) {
            ConsumeInputVector();
            bHasRequestedVelocity = false;
            RequestedVelocity = glm::vec3(0.0f);
            return;
        }
        if (bPressedJump)
            DoJump();

        ResolvePenetration();

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
        ACharacter* character = GetCharacter();
        if (!character || !character->IsLocallyControlled() || !character->HasNetServerTransform())
            return;

        const glm::vec3 serverLoc = character->GetNetServerLocation();
        const glm::vec3 clientLoc = character->GetActorLocation();
        const glm::vec3 delta = serverLoc - clientLoc;
        const float errSq = glm::dot(delta, delta);
        constexpr float kHardSnapSq = 0.25f * 0.25f;
        constexpr float kSoftCorrectSq = 0.05f * 0.05f;

        if (errSq > kHardSnapSq) {
            character->SetActorLocation(serverLoc);
            character->SetActorRotation(character->GetNetServerRotation());
        } else if (errSq > kSoftCorrectSq) {
            const float alpha = std::min(1.0f, DeltaSeconds * 12.0f);
            character->SetActorLocation(glm::mix(clientLoc, serverLoc, alpha));
        }
    }

    void UCharacterMovementComponent::Tick(float DeltaSeconds) {
        // ACharacter drives TickMovement after input so camera/anim see this frame's pose.
        if (GetCharacter())
            return;
        auto* pawn = GetPawnOwner();
        if (!pawn)
            return;
        if (!pawn->HasAuthority() && pawn->GetLocalRole() == ENetRole::SimulatedProxy)
            return;
        TickMovement(DeltaSeconds);
    }

} // namespace Leon
