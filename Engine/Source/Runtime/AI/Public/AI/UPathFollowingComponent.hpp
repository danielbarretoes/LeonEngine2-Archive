#pragma once

#include "Gameplay/UActorComponent.hpp"
#include "AI/FNavTypes.hpp"

namespace Leon {

    class ACharacter;

    /**
     * Follows an FNavPath by issuing RequestDirectMove toward the current waypoint.
     */
    class UPathFollowingComponent : public UActorComponent {
    public:
        UPathFollowingComponent(const std::string& InName = "PathFollowing");

        void RequestMove(const FNavPath& InPath, float InAcceptanceRadius);
        void AbortMove();
        void TickPath(float DeltaSeconds);

        bool IsMoving() const { return Status == EPathFollowingStatus::Moving; }
        EPathFollowingStatus GetStatus() const { return Status; }
        const FNavPath& GetPath() const { return Path; }
        int32_t GetCurrentWaypoint() const { return WaypointIndex; }
        glm::vec3 GetCurrentDestination() const;
        float GetRemainingDistance() const;

    private:
        ACharacter* GetCharacter() const;
        void AdvanceWaypoint();

        FNavPath Path;
        EPathFollowingStatus Status = EPathFollowingStatus::Idle;
        int32_t WaypointIndex = 0;
        float AcceptanceRadius = 0.6f;
        glm::vec3 LastLocation{0.0f};
        float StuckTime = 0.0f;
    };

} // namespace Leon
