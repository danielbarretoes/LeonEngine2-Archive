#include "Gameplay/AAIController.hpp"
#include "AI/UBehaviorTree.hpp"
#include "AI/UNavigationSystem.hpp"
#include "Gameplay/ACharacter.hpp"
#include "Gameplay/UCharacterMovementComponent.hpp"
#include "Engine/UWorld.hpp"

namespace Leon {

    AAIController::AAIController(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AController(InHandle, InWorld, InName) {
        SetClass("AAIController");
    }

    void AAIController::PostInitializeComponents() {
        if (!Blackboard)
            Blackboard = AddActorComponent<UBlackboardComponent>("Blackboard");
        if (!Brain)
            Brain = AddActorComponent<UBehaviorTreeComponent>("Brain");
        if (!PathFollowing)
            PathFollowing = AddActorComponent<UPathFollowingComponent>("PathFollowing");
        if (!Perception)
            Perception = AddActorComponent<UAIPerceptionComponent>("Perception");
        if (Brain) {
            Brain->SetBlackboard(Blackboard);
            Brain->SetComponentTickEnabled(false);
        }
        if (PathFollowing)
            PathFollowing->SetComponentTickEnabled(false);
        if (Perception)
            Perception->SetBlackboard(Blackboard.get());
    }

    void AAIController::UseBlackboard(const TRef<UBlackboardData>& InAsset) {
        if (!Blackboard)
            Blackboard = AddActorComponent<UBlackboardComponent>("Blackboard");
        Blackboard->InitializeFrom(InAsset);
        if (Brain)
            Brain->SetBlackboard(Blackboard);
        if (Perception)
            Perception->SetBlackboard(Blackboard.get());
    }

    bool AAIController::RunBehaviorTree(const TRef<UBehaviorTree>& InTree) {
        if (!Brain)
            Brain = AddActorComponent<UBehaviorTreeComponent>("Brain");
        if (InTree && InTree->GetBlackboardAsset())
            UseBlackboard(InTree->GetBlackboardAsset());
        Brain->SetBlackboard(Blackboard);
        Brain->SetComponentTickEnabled(false);
        Brain->StartTree(InTree);
        return true;
    }

    void AAIController::SetSightConfig(const FAISightConfig& InConfig) {
        if (!Perception)
            Perception = AddActorComponent<UAIPerceptionComponent>("Perception");
        Perception->SetSightConfig(InConfig);
    }

    const std::vector<AActor*>& AAIController::GetPerceivedActors() const {
        static const std::vector<AActor*> kEmpty;
        return Perception ? Perception->GetPerceivedActors() : kEmpty;
    }

    bool AAIController::HasLineOfSightTo(AActor& InTarget) const {
        return Perception && Perception->HasLineOfSight(InTarget);
    }

    void AAIController::MoveToLocation(const glm::vec3& InDest, float InAcceptanceRadius) {
        MoveDestination = InDest;
        MoveGoal = nullptr;
        MoveAcceptanceRadius = InAcceptanceRadius;
        bHasMoveRequest = true;

        auto* character = GetPawn<ACharacter>();
        if (!character || !character->GetCharacterMovement())
            return;

        glm::vec3 to = InDest - character->GetActorLocation();
        to.y = 0.0f;
        if (glm::length(to) <= InAcceptanceRadius) {
            StopMovement();
            return;
        }

        if (PathFollowing && PathFollowing->IsMoving() && bHasPathedGoal) {
            glm::vec3 delta = InDest - LastPathedGoal;
            delta.y = 0.0f;
            if (glm::length(delta) < 1.2f)
                return;
        }

        UNavigationSystem* nav = World ? World->GetNavigationSystem() : nullptr;
        if (nav && nav->IsBuilt() && PathFollowing) {
            FNavPath path = nav->FindPath(character->GetActorLocation(), InDest);
            if (!path.IsValid()) {
                path.Points = {character->GetActorLocation(), InDest};
                path.Status = ENavPathStatus::Partial;
                path.Length = glm::length(to);
            }
            PathFollowing->RequestMove(path, InAcceptanceRadius);
            LastPathedGoal = InDest;
            bHasPathedGoal = true;
            return;
        }

        const glm::vec3 vel = glm::normalize(to) * character->GetCharacterMovement()->GetMaxWalkSpeed();
        character->GetCharacterMovement()->RequestDirectMove(vel, true);
    }

    void AAIController::StopMovement() {
        AController::StopMovement();
        bHasPathedGoal = false;
        if (PathFollowing)
            PathFollowing->AbortMove();
    }

    EPathFollowingStatus AAIController::GetMoveStatus() const {
        return PathFollowing ? PathFollowing->GetStatus() : EPathFollowingStatus::Idle;
    }

    ENavPathStatus AAIController::GetPathStatus() const {
        return PathFollowing ? PathFollowing->GetPath().Status : ENavPathStatus::Invalid;
    }

    float AAIController::GetPathLength() const {
        return PathFollowing ? PathFollowing->GetPath().Length : 0.0f;
    }

    void AAIController::Tick(float DeltaSeconds) {
        if (PathFollowing && PathFollowing->IsMoving()) {
            PathFollowing->TickPath(DeltaSeconds);
            if (PathFollowing->GetStatus() == EPathFollowingStatus::Success)
                bHasMoveRequest = false;
            else if (PathFollowing->GetStatus() == EPathFollowingStatus::Failed)
                bHasPathedGoal = false;
        } else if (bHasMoveRequest) {
            if (MoveGoal)
                MoveToActor(MoveGoal, MoveAcceptanceRadius);
            else
                MoveToLocation(MoveDestination, MoveAcceptanceRadius);
        }
        if (Brain)
            Brain->Tick(DeltaSeconds);
    }

} // namespace Leon
