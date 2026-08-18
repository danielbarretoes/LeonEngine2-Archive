#pragma once

#include "Gameplay/AController.hpp"
#include "AI/UBehaviorTreeComponent.hpp"
#include "AI/UBlackboardComponent.hpp"
#include "AI/UPathFollowingComponent.hpp"
#include "AI/UAIPerceptionComponent.hpp"
#include "AI/FNavTypes.hpp"

#include <vector>

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
        TRef<UAIPerceptionComponent> GetPerceptionComponent() const { return Perception; }

        void SetSightConfig(const FAISightConfig& InConfig);
        const std::vector<AActor*>& GetPerceivedActors() const;
        bool HasLineOfSightTo(AActor& InTarget) const;

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
        TRef<UAIPerceptionComponent> Perception;
        glm::vec3 LastPathedGoal{0.0f};
        bool bHasPathedGoal = false;
    };

} // namespace Leon
