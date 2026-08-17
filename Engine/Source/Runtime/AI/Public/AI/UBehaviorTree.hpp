#pragma once

#include "Core/Base.hpp"
#include "Gameplay/UObject.hpp"
#include "AI/UBlackboardData.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Leon {

    class UBehaviorTreeComponent;
    class UBTCompositeNode;

    enum class EBTNodeResult : uint8_t { Succeeded = 0, Failed = 1, InProgress = 2, Aborted = 3 };

    class UBTNode : public UObject {
    public:
        explicit UBTNode(const std::string& InName = "BTNode");
        virtual ~UBTNode() override = default;
        void SetParent(UBTNode* InParent) { Parent = InParent; }
        UBTNode* GetParent() const { return Parent; }

    protected:
        UBTNode* Parent = nullptr;
    };

    class UBTDecorator : public UBTNode {
    public:
        explicit UBTDecorator(const std::string& InName = "BTDecorator");
        virtual bool CanExecute(UBehaviorTreeComponent& InOwner) const = 0;
    };

    class UBTService : public UBTNode {
    public:
        explicit UBTService(const std::string& InName = "BTService");
        virtual void TickService(UBehaviorTreeComponent& InOwner, float DeltaSeconds) = 0;
        float Interval = 0.2f;
        float TimeAccumulator = 0.0f;
    };

    class UBTTaskNode : public UBTNode {
    public:
        explicit UBTTaskNode(const std::string& InName = "BTTask");
        virtual EBTNodeResult ExecuteTask(UBehaviorTreeComponent& InOwner, float DeltaSeconds) = 0;
        virtual void AbortTask(UBehaviorTreeComponent& InOwner) { (void)InOwner; }
        std::vector<TRef<UBTDecorator>> Decorators;
        std::vector<TRef<UBTService>> Services;
    };

    class UBTCompositeNode : public UBTNode {
    public:
        explicit UBTCompositeNode(const std::string& InName = "BTComposite");
        void AddChild(const TRef<UBTNode>& InChild);
        const std::vector<TRef<UBTNode>>& GetChildren() const { return Children; }
        std::vector<TRef<UBTDecorator>> Decorators;
        std::vector<TRef<UBTService>> Services;

    protected:
        std::vector<TRef<UBTNode>> Children;
    };

    class UBTComposite_Selector : public UBTCompositeNode {
    public:
        UBTComposite_Selector(const std::string& InName = "Selector");
    };

    class UBTComposite_Sequence : public UBTCompositeNode {
    public:
        UBTComposite_Sequence(const std::string& InName = "Sequence");
    };

    class UBTDecorator_Blackboard : public UBTDecorator {
    public:
        UBTDecorator_Blackboard(const std::string& InKey, bool bExpected);
        bool CanExecute(UBehaviorTreeComponent& InOwner) const override;

    private:
        std::string Key;
        bool bExpectedValue = true;
    };

    class UBTTask_Wait : public UBTTaskNode {
    public:
        explicit UBTTask_Wait(float InWaitTime);
        EBTNodeResult ExecuteTask(UBehaviorTreeComponent& InOwner, float DeltaSeconds) override;

    private:
        float WaitTime = 0.2f;
        float Elapsed = 0.0f;
    };

    class UBTTask_MoveTo : public UBTTaskNode {
    public:
        UBTTask_MoveTo(const std::string& InLocationKey, float InAcceptanceRadius = 0.6f);
        EBTNodeResult ExecuteTask(UBehaviorTreeComponent& InOwner, float DeltaSeconds) override;

    private:
        std::string LocationKey;
        float AcceptanceRadius = 0.6f;
    };

    class UBTTask_MoveToActor : public UBTTaskNode {
    public:
        UBTTask_MoveToActor(const std::string& InActorKey, float InAcceptanceRadius = 0.6f);
        EBTNodeResult ExecuteTask(UBehaviorTreeComponent& InOwner, float DeltaSeconds) override;

    private:
        std::string ActorKey;
        float AcceptanceRadius = 0.6f;
    };

    using FBTNativeTask = std::function<EBTNodeResult(UBehaviorTreeComponent&, float)>;

    class UBTTask_Native : public UBTTaskNode {
    public:
        UBTTask_Native(const std::string& InName, FBTNativeTask InFn);
        EBTNodeResult ExecuteTask(UBehaviorTreeComponent& InOwner, float DeltaSeconds) override;

    private:
        FBTNativeTask Fn;
    };

    class UBTService_Native : public UBTService {
    public:
        UBTService_Native(const std::string& InName, std::function<void(UBehaviorTreeComponent&, float)> InFn);
        void TickService(UBehaviorTreeComponent& InOwner, float DeltaSeconds) override;

    private:
        std::function<void(UBehaviorTreeComponent&, float)> Fn;
    };

    class UBehaviorTree : public UObject {
    public:
        UBehaviorTree(const std::string& InName = "BehaviorTree");

        void SetRoot(const TRef<UBTCompositeNode>& InRoot) { Root = InRoot; }
        TRef<UBTCompositeNode> GetRoot() const { return Root; }
        void SetBlackboardAsset(const TRef<UBlackboardData>& InAsset) { BlackboardAsset = InAsset; }
        TRef<UBlackboardData> GetBlackboardAsset() const { return BlackboardAsset; }

        bool SaveToFile(const std::string& InPath) const;
        bool LoadFromFile(const std::string& InPath);

        static constexpr uint32_t Magic = 0x5256424C; // 'LBVR'
        static constexpr uint32_t Version = 1;

    private:
        TRef<UBTCompositeNode> Root;
        TRef<UBlackboardData> BlackboardAsset;
        std::string BlackboardAssetPath;
    };

} // namespace Leon
