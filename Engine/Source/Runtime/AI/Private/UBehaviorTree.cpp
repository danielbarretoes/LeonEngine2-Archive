#include "AI/UBehaviorTree.hpp"
#include "AI/UBehaviorTreeComponent.hpp"
#include "Gameplay/AAIController.hpp"
#include "Gameplay/APawn.hpp"
#include "Gameplay/ACharacter.hpp"

#include <fstream>

namespace Leon {

    UBTNode::UBTNode(const std::string& InName) : UObject(InName) {}
    UBTDecorator::UBTDecorator(const std::string& InName) : UBTNode(InName) {}
    UBTService::UBTService(const std::string& InName) : UBTNode(InName) {}
    UBTTaskNode::UBTTaskNode(const std::string& InName) : UBTNode(InName) {}
    UBTCompositeNode::UBTCompositeNode(const std::string& InName) : UBTNode(InName) {}
    void UBTCompositeNode::AddChild(const TRef<UBTNode>& InChild) {
        if (InChild) {
            InChild->SetParent(this);
            Children.push_back(InChild);
        }
    }
    UBTComposite_Selector::UBTComposite_Selector(const std::string& InName) : UBTCompositeNode(InName) {}
    UBTComposite_Sequence::UBTComposite_Sequence(const std::string& InName) : UBTCompositeNode(InName) {}

    UBTDecorator_Blackboard::UBTDecorator_Blackboard(const std::string& InKey, bool bExpected)
        : UBTDecorator("Blackboard"), Key(InKey), bExpectedValue(bExpected) {}

    bool UBTDecorator_Blackboard::CanExecute(UBehaviorTreeComponent& InOwner) const {
        auto board = InOwner.GetBlackboard();
        return board && board->GetValueAsBool(Key) == bExpectedValue;
    }

    UBTTask_Wait::UBTTask_Wait(float InWaitTime) : UBTTaskNode("Wait"), WaitTime(InWaitTime) {}

    EBTNodeResult UBTTask_Wait::ExecuteTask(UBehaviorTreeComponent& InOwner, float DeltaSeconds) {
        (void)InOwner;
        Elapsed += DeltaSeconds;
        if (Elapsed >= WaitTime) {
            Elapsed = 0.0f;
            return EBTNodeResult::Succeeded;
        }
        return EBTNodeResult::InProgress;
    }

    UBTTask_MoveTo::UBTTask_MoveTo(const std::string& InLocationKey, float InAcceptanceRadius)
        : UBTTaskNode("MoveTo"), LocationKey(InLocationKey), AcceptanceRadius(InAcceptanceRadius) {}

    EBTNodeResult UBTTask_MoveTo::ExecuteTask(UBehaviorTreeComponent& InOwner, float DeltaSeconds) {
        (void)DeltaSeconds;
        auto* ai = InOwner.GetAIOwner();
        auto board = InOwner.GetBlackboard();
        if (!ai || !board)
            return EBTNodeResult::Failed;
        glm::vec3 dest = board->GetValueAsVector(LocationKey);
        ai->MoveToLocation(dest, AcceptanceRadius);
        auto* pawn = ai->GetPawn();
        if (!pawn)
            return EBTNodeResult::Failed;
        glm::vec3 to = dest - pawn->GetActorLocation();
        to.y = 0.0f;
        if (glm::length(to) <= AcceptanceRadius)
            return EBTNodeResult::Succeeded;
        return EBTNodeResult::InProgress;
    }

    UBTTask_MoveToActor::UBTTask_MoveToActor(const std::string& InActorKey, float InAcceptanceRadius)
        : UBTTaskNode("MoveToActor"), ActorKey(InActorKey), AcceptanceRadius(InAcceptanceRadius) {}

    EBTNodeResult UBTTask_MoveToActor::ExecuteTask(UBehaviorTreeComponent& InOwner, float DeltaSeconds) {
        (void)DeltaSeconds;
        auto* ai = InOwner.GetAIOwner();
        auto board = InOwner.GetBlackboard();
        if (!ai || !board)
            return EBTNodeResult::Failed;
        auto* goal = static_cast<AActor*>(board->GetValueAsObject(ActorKey));
        if (!goal)
            return EBTNodeResult::Failed;
        ai->MoveToActor(goal, AcceptanceRadius);
        auto* pawn = ai->GetPawn();
        if (!pawn)
            return EBTNodeResult::Failed;
        glm::vec3 to = goal->GetActorLocation() - pawn->GetActorLocation();
        to.y = 0.0f;
        if (glm::length(to) <= AcceptanceRadius)
            return EBTNodeResult::Succeeded;
        return EBTNodeResult::InProgress;
    }

    UBTTask_Native::UBTTask_Native(const std::string& InName, FBTNativeTask InFn) : UBTTaskNode(InName), Fn(std::move(InFn)) {}

    EBTNodeResult UBTTask_Native::ExecuteTask(UBehaviorTreeComponent& InOwner, float DeltaSeconds) {
        return Fn ? Fn(InOwner, DeltaSeconds) : EBTNodeResult::Failed;
    }

    UBTService_Native::UBTService_Native(const std::string& InName,
                                         std::function<void(UBehaviorTreeComponent&, float)> InFn)
        : UBTService(InName), Fn(std::move(InFn)) {}

    void UBTService_Native::TickService(UBehaviorTreeComponent& InOwner, float DeltaSeconds) {
        if (Fn)
            Fn(InOwner, DeltaSeconds);
    }

    UBehaviorTree::UBehaviorTree(const std::string& InName) : UObject(InName) {}

    bool UBehaviorTree::SaveToFile(const std::string& InPath) const {
        std::ofstream out(InPath, std::ios::trunc);
        if (!out)
            return false;
        out << "LBEHAVIOR " << Version << "\n";
        out << "Root " << (Root ? Root->GetName() : "None") << "\n";
        return true;
    }

    bool UBehaviorTree::LoadFromFile(const std::string& InPath) {
        std::ifstream in(InPath);
        return static_cast<bool>(in);
    }

} // namespace Leon
