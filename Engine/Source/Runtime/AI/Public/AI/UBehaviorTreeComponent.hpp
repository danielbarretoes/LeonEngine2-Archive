#pragma once

#include "Gameplay/UActorComponent.hpp"
#include "AI/UBehaviorTree.hpp"
#include "AI/UBlackboardData.hpp"

namespace Leon {

    class AAIController;

    class UBehaviorTreeComponent : public UActorComponent {
    public:
        UBehaviorTreeComponent(const std::string& InName = "BehaviorTreeComponent");

        void Tick(float DeltaSeconds) override;

        void StartTree(const TRef<UBehaviorTree>& InTree);
        void StopTree();
        bool IsRunning() const { return bRunning && Tree != nullptr; }

        TRef<UBehaviorTree> GetTree() const { return Tree; }
        TRef<UBlackboardComponent> GetBlackboard() const { return Blackboard; }
        void SetBlackboard(const TRef<UBlackboardComponent>& InBoard) { Blackboard = InBoard; }

        AAIController* GetAIOwner() const;
        const std::string& GetActiveNodeName() const { return ActiveNodeName; }

    private:
        EBTNodeResult ExecuteNode(UBTNode* InNode, float DeltaSeconds);
        bool DecoratorsAllow(const std::vector<TRef<UBTDecorator>>& InDecorators) const;
        void TickServices(const std::vector<TRef<UBTService>>& InServices, float DeltaSeconds);

        TRef<UBehaviorTree> Tree;
        TRef<UBlackboardComponent> Blackboard;
        bool bRunning = false;
        std::string ActiveNodeName = "None";
        int32_t SelectorIndex = 0;
        int32_t SequenceIndex = 0;
        UBTNode* ActiveTask = nullptr;
    };

} // namespace Leon
