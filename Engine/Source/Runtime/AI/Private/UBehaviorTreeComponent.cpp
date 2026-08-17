#include "AI/UBehaviorTreeComponent.hpp"
#include "Gameplay/AAIController.hpp"

namespace Leon {

    UBehaviorTreeComponent::UBehaviorTreeComponent(const std::string& InName) : UActorComponent(InName) {}

    AAIController* UBehaviorTreeComponent::GetAIOwner() const {
        return Owner ? dynamic_cast<AAIController*>(Owner) : nullptr;
    }

    void UBehaviorTreeComponent::StartTree(const TRef<UBehaviorTree>& InTree) {
        Tree = InTree;
        bRunning = Tree != nullptr;
        ActiveTask = nullptr;
        SelectorIndex = 0;
        SequenceIndex = 0;
        ActiveNodeName = "Root";
        if (Tree && Tree->GetBlackboardAsset() && Blackboard)
            Blackboard->InitializeFrom(Tree->GetBlackboardAsset());
    }

    void UBehaviorTreeComponent::StopTree() {
        bRunning = false;
        ActiveTask = nullptr;
        ActiveNodeName = "None";
    }

    bool UBehaviorTreeComponent::DecoratorsAllow(const std::vector<TRef<UBTDecorator>>& InDecorators) const {
        for (const auto& dec : InDecorators) {
            if (dec && !dec->CanExecute(const_cast<UBehaviorTreeComponent&>(*this)))
                return false;
        }
        return true;
    }

    void UBehaviorTreeComponent::TickServices(const std::vector<TRef<UBTService>>& InServices, float DeltaSeconds) {
        for (const auto& svc : InServices) {
            if (!svc)
                continue;
            svc->TimeAccumulator += DeltaSeconds;
            if (svc->TimeAccumulator >= svc->Interval) {
                svc->TimeAccumulator = 0.0f;
                svc->TickService(*this, DeltaSeconds);
            }
        }
    }

    EBTNodeResult UBehaviorTreeComponent::ExecuteNode(UBTNode* InNode, float DeltaSeconds) {
        if (!InNode)
            return EBTNodeResult::Failed;
        ActiveNodeName = InNode->GetName();

        if (auto* selector = dynamic_cast<UBTComposite_Selector*>(InNode)) {
            TickServices(selector->Services, DeltaSeconds);
            if (!DecoratorsAllow(selector->Decorators))
                return EBTNodeResult::Failed;
            for (const auto& child : selector->GetChildren()) {
                EBTNodeResult r = ExecuteNode(child.get(), DeltaSeconds);
                if (r == EBTNodeResult::Succeeded || r == EBTNodeResult::InProgress)
                    return r;
            }
            return EBTNodeResult::Failed;
        }

        if (auto* sequence = dynamic_cast<UBTComposite_Sequence*>(InNode)) {
            TickServices(sequence->Services, DeltaSeconds);
            if (!DecoratorsAllow(sequence->Decorators))
                return EBTNodeResult::Failed;
            for (const auto& child : sequence->GetChildren()) {
                EBTNodeResult r = ExecuteNode(child.get(), DeltaSeconds);
                if (r != EBTNodeResult::Succeeded)
                    return r;
            }
            return EBTNodeResult::Succeeded;
        }

        if (auto* task = dynamic_cast<UBTTaskNode*>(InNode)) {
            TickServices(task->Services, DeltaSeconds);
            if (!DecoratorsAllow(task->Decorators))
                return EBTNodeResult::Failed;
            return task->ExecuteTask(*this, DeltaSeconds);
        }

        return EBTNodeResult::Failed;
    }

    void UBehaviorTreeComponent::Tick(float DeltaSeconds) {
        if (!bRunning || !Tree || !Tree->GetRoot())
            return;
        ExecuteNode(Tree->GetRoot().get(), DeltaSeconds);
    }

} // namespace Leon
