#pragma once

#include "Gameplay/AController.hpp"
#include "AI/UBehaviorTreeComponent.hpp"
#include "AI/UBlackboardComponent.hpp"
#include "AI/UPathFollowingComponent.hpp"
#include "AI/FNavTypes.hpp"

namespace Leon {

    class UBehaviorTree;
    class UBlackboardData;

    /**
     * Generic AI controller. Game subclasses own the tree asset, not the engine.
     */
    class AAIController : public AController {
    public:
        AAIController() = default;
        AAIController(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "AIController");

        void PostInitializeComponents() override;
        void Tick(float DeltaSeconds) override;

        bool RunBehaviorTree(const TRef<UBehaviorTree>& InTree);
        TRef<UBehaviorTreeComponent> GetBrainComponent() const { return Brain; }
        TRef<UBlackboardComponent> GetBlackboardComponent() const { return Blackboard; }
        TRef<UPathFollowingComponent> GetPathFollowingComponent() const { return PathFollowing; }

        void UseBlackboard(const TRef<UBlackboardData>& InAsset);

        void MoveToLocation(const glm::vec3& InDest, float InAcceptanceRadius = 0.6f) override;
        void StopMovement() override;

        EPathFollowingStatus GetMoveStatus() const;
        ENavPathStatus GetPathStatus() const;
        float GetPathLength() const;

    protected:
        TRef<UBehaviorTreeComponent> Brain;
        TRef<UBlackboardComponent> Blackboard;
        TRef<UPathFollowingComponent> PathFollowing;
        glm::vec3 LastPathedGoal{0.0f};
        bool bHasPathedGoal = false;
    };

} // namespace Leon
