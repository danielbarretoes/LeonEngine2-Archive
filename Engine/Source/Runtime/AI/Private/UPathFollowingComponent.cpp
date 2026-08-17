#include "AI/UPathFollowingComponent.hpp"
#include "Gameplay/AAIController.hpp"
#include "Gameplay/ACharacter.hpp"
#include "Gameplay/UCharacterMovementComponent.hpp"

#include <algorithm>
#include <cmath>

namespace Leon {

    UPathFollowingComponent::UPathFollowingComponent(const std::string& InName) : UActorComponent(InName) {
        SetComponentTickEnabled(false);
    }

    ACharacter* UPathFollowingComponent::GetCharacter() const {
        auto* ai = Owner ? dynamic_cast<AAIController*>(Owner) : nullptr;
        return ai ? ai->GetPawn<ACharacter>() : nullptr;
    }

    glm::vec3 UPathFollowingComponent::GetCurrentDestination() const {
        if (Path.Points.empty())
            return glm::vec3(0.0f);
        const int32_t i = std::clamp(WaypointIndex, 0, Path.NumPoints() - 1);
        return Path.Points[static_cast<size_t>(i)];
    }

    float UPathFollowingComponent::GetRemainingDistance() const {
        auto* character = GetCharacter();
        if (!character || Path.Points.empty())
            return 0.0f;
        float dist = 0.0f;
        glm::vec3 prev = character->GetActorLocation();
        for (int32_t i = WaypointIndex; i < Path.NumPoints(); ++i) {
            glm::vec3 d = Path.Points[static_cast<size_t>(i)] - prev;
            d.y = 0.0f;
            dist += glm::length(d);
            prev = Path.Points[static_cast<size_t>(i)];
        }
        return dist;
    }

    void UPathFollowingComponent::RequestMove(const FNavPath& InPath, float InAcceptanceRadius) {
        Path = InPath;
        AcceptanceRadius = std::max(InAcceptanceRadius, 0.15f);
        WaypointIndex = 0;
        StuckTime = 0.0f;
        if (auto* character = GetCharacter())
            LastLocation = character->GetActorLocation();
        if (!Path.IsValid()) {
            Status = EPathFollowingStatus::Failed;
            return;
        }
        Status = EPathFollowingStatus::Moving;
        AdvanceWaypoint();
    }

    void UPathFollowingComponent::AbortMove() {
        Path = {};
        Status = EPathFollowingStatus::Idle;
        WaypointIndex = 0;
        if (auto* character = GetCharacter()) {
            if (auto move = character->GetCharacterMovement())
                move->StopActiveMovement();
        }
    }

    void UPathFollowingComponent::AdvanceWaypoint() {
        auto* character = GetCharacter();
        if (!character)
            return;
        while (WaypointIndex < Path.NumPoints()) {
            glm::vec3 to = Path.Points[static_cast<size_t>(WaypointIndex)] - character->GetActorLocation();
            to.y = 0.0f;
            const float accept = (WaypointIndex == Path.NumPoints() - 1) ? AcceptanceRadius : 0.45f;
            if (glm::length(to) <= accept) {
                ++WaypointIndex;
                continue;
            }
            break;
        }
    }

    void UPathFollowingComponent::TickPath(float DeltaSeconds) {
        if (Status != EPathFollowingStatus::Moving)
            return;
        auto* character = GetCharacter();
        auto move = character ? character->GetCharacterMovement() : nullptr;
        if (!character || !move) {
            Status = EPathFollowingStatus::Failed;
            return;
        }

        AdvanceWaypoint();
        if (WaypointIndex >= Path.NumPoints()) {
            move->StopActiveMovement();
            Status = EPathFollowingStatus::Success;
            return;
        }

        const glm::vec3 dest = Path.Points[static_cast<size_t>(WaypointIndex)];
        glm::vec3 to = dest - character->GetActorLocation();
        to.y = 0.0f;
        const float dist = glm::length(to);
        if (dist <= AcceptanceRadius && WaypointIndex >= Path.NumPoints() - 1) {
            move->StopActiveMovement();
            Status = EPathFollowingStatus::Success;
            return;
        }

        glm::vec3 planar = character->GetActorLocation() - LastLocation;
        planar.y = 0.0f;
        if (glm::length(planar) < 0.08f)
            StuckTime += DeltaSeconds;
        else
            StuckTime = 0.0f;
        LastLocation = character->GetActorLocation();
        if (StuckTime > 1.2f) {
            StuckTime = 0.0f;
            ++WaypointIndex;
            if (WaypointIndex >= Path.NumPoints()) {
                Status = EPathFollowingStatus::Failed;
                move->StopActiveMovement();
                return;
            }
        }

        if (dist > 1e-4f) {
            const glm::vec3 vel = (to / dist) * move->GetMaxWalkSpeed();
            move->RequestDirectMove(vel, true);
        }
        (void)DeltaSeconds;
    }

} // namespace Leon
