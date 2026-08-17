#include "Gameplay/FAnimStateMachine.hpp"
#include "Gameplay/UAnimInstance.hpp"
#include "Assets/FAnimRuntime.hpp"

#include <algorithm>
#include <cstdlib>

namespace Leon {

    namespace {
        bool StartsWith(const std::string& InStr, const char* InPrefix) {
            const size_t n = std::char_traits<char>::length(InPrefix);
            return InStr.size() >= n && InStr.compare(0, n, InPrefix) == 0;
        }
    } // namespace

    void FAnimStateMachine::AddState(FAnimState InState) {
        if (CurrentState.empty())
            CurrentState = InState.Name;
        States.push_back(std::move(InState));
    }

    void FAnimStateMachine::AddTransition(FAnimTransition InTransition) {
        Transitions.push_back(std::move(InTransition));
    }

    void FAnimStateMachine::SetDefaultState(const std::string& InName) {
        CurrentState = InName;
    }

    void FAnimStateMachine::Reset() {
        StateTime = 0.0f;
        TransitionAlpha = 1.0f;
        TransitionBlendTime = 0.0f;
        PreviousState.clear();
        PreviousPose = {};
        if (!States.empty() && CurrentState.empty())
            CurrentState = States.front().Name;
    }

    const FAnimState* FAnimStateMachine::FindState(const std::string& InName) const {
        for (const auto& state : States) {
            if (state.Name == InName)
                return &state;
        }
        return nullptr;
    }

    bool FAnimStateMachine::EvaluateCondition(const std::string& InName, const UAnimInstance& InInstance) const {
        if (StartsWith(InName, "Bool:"))
            return InInstance.GetBool(InName.substr(5));
        if (StartsWith(InName, "NotBool:"))
            return !InInstance.GetBool(InName.substr(8));
        if (StartsWith(InName, "FloatGreater:")) {
            const std::string rest = InName.substr(13);
            const size_t colon = rest.find(':');
            if (colon == std::string::npos)
                return false;
            const float threshold = std::strtof(rest.c_str() + colon + 1, nullptr);
            return InInstance.GetFloat(rest.substr(0, colon)) > threshold;
        }
        if (StartsWith(InName, "FloatLessEqual:")) {
            const std::string rest = InName.substr(15);
            const size_t colon = rest.find(':');
            if (colon == std::string::npos)
                return false;
            const float threshold = std::strtof(rest.c_str() + colon + 1, nullptr);
            return InInstance.GetFloat(rest.substr(0, colon)) <= threshold;
        }
        return false;
    }

    void FAnimStateMachine::Update(float InDeltaSeconds, const UAnimInstance& InInstance) {
        StateTime += InDeltaSeconds;
        if (TransitionAlpha < 1.0f && TransitionBlendTime > 1e-4f) {
            TransitionAlpha = glm::clamp(TransitionAlpha + InDeltaSeconds / TransitionBlendTime, 0.0f, 1.0f);
            if (TransitionAlpha >= 1.0f)
                PreviousState.clear();
        }

        for (const auto& tr : Transitions) {
            if (tr.FromState != CurrentState && tr.FromState != "*")
                continue;
            if (!EvaluateCondition(tr.ConditionName, InInstance))
                continue;
            if (tr.ToState == CurrentState)
                continue;
            PreviousState = CurrentState;
            CurrentState = tr.ToState;
            StateTime = 0.0f;
            TransitionBlendTime = std::max(tr.BlendTime, 0.0f);
            TransitionAlpha = TransitionBlendTime > 1e-4f ? 0.0f : 1.0f;
            break;
        }
    }

    void FAnimStateMachine::Evaluate(const USkeleton& InSkeleton, const UAnimInstance& InInstance,
                                     FPose& OutPose) const {
        const FAnimState* state = FindState(CurrentState);
        if (!state) {
            FAnimRuntime::RestPose(InSkeleton, OutPose);
            return;
        }

        auto evalState = [&](const FAnimState& InState, FPose& Out) {
            if (InState.Type == EAnimNodeType::BlendSpace && InState.BlendSpace) {
                FAnimRuntime::SampleBlendSpace(*InState.BlendSpace, InSkeleton,
                                               glm::vec2(InInstance.GetFloat(InInstance.GetBlendParamX()),
                                                         InInstance.GetFloat(InInstance.GetBlendParamY())),
                                               StateTime, InState.bLoop, Out);
            } else if (InState.Sequence) {
                FAnimRuntime::SampleSequence(*InState.Sequence, InSkeleton, StateTime, InState.bLoop, Out);
            } else {
                FAnimRuntime::RestPose(InSkeleton, Out);
            }
        };

        evalState(*state, OutPose);
        if (TransitionAlpha < 1.0f && !PreviousState.empty()) {
            const FAnimState* prev = FindState(PreviousState);
            if (prev) {
                FPose fromPose;
                evalState(*prev, fromPose);
                FPose blended;
                FAnimRuntime::BlendPoses(fromPose, OutPose, TransitionAlpha, blended);
                OutPose = std::move(blended);
            }
        }
    }

} // namespace Leon
