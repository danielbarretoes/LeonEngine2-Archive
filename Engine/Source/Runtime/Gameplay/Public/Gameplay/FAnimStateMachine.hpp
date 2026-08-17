#pragma once

#include "Assets/FAnimTypes.hpp"
#include "Assets/UAnimSequence.hpp"
#include "Assets/UBlendSpace.hpp"
#include "Assets/USkeleton.hpp"

#include <string>
#include <vector>

namespace Leon {

    class UAnimInstance;

    enum class EAnimNodeType : uint8_t { Sequence = 0, BlendSpace = 1 };

    struct FAnimState {
        std::string Name;
        EAnimNodeType Type = EAnimNodeType::Sequence;
        TRef<UAnimSequence> Sequence;
        TRef<UBlendSpace> BlendSpace;
        bool bLoop = true;
    };

    /**
     * ConditionName uses generic parameter queries on UAnimInstance:
     *   Bool:<name>  NotBool:<name>
     *   FloatGreater:<name>:<threshold>  FloatLessEqual:<name>:<threshold>
     *   TimeGreater:<seconds>  (current state time)
     */
    struct FAnimTransition {
        std::string FromState;
        std::string ToState;
        std::string ConditionName;
        float BlendTime = 0.15f;
    };

    class FAnimStateMachine {
    public:
        void AddState(FAnimState InState);
        void AddTransition(FAnimTransition InTransition);
        void SetDefaultState(const std::string& InName);

        const std::string& GetCurrentState() const { return CurrentState; }
        float GetStateTime() const { return StateTime; }

        void Reset();
        void ResetToDefault();
        void Update(float InDeltaSeconds, const UAnimInstance& InInstance);
        void Evaluate(const USkeleton& InSkeleton, const UAnimInstance& InInstance, FPose& OutPose) const;

        std::vector<FAnimState>& GetStates() { return States; }
        const std::vector<FAnimState>& GetStates() const { return States; }

    private:
        const FAnimState* FindState(const std::string& InName) const;
        bool EvaluateCondition(const std::string& InName, const UAnimInstance& InInstance) const;

        std::vector<FAnimState> States;
        std::vector<FAnimTransition> Transitions;
        std::string CurrentState;
        std::string DefaultState;
        std::string PreviousState;
        float StateTime = 0.0f;
        float TransitionAlpha = 1.0f;
        float TransitionBlendTime = 0.0f;
        FPose PreviousPose;
    };

} // namespace Leon
