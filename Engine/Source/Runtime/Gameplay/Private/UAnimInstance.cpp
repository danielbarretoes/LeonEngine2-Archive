#include "Gameplay/UAnimInstance.hpp"
#include "Assets/FAnimRuntime.hpp"

namespace Leon {

    UAnimInstance::UAnimInstance(const std::string& InName) : UObject(InName) {}

    void UAnimInstance::Initialize(const TRef<USkeleton>& InSkeleton) {
        Skeleton = InSkeleton;
        if (Skeleton)
            UpperBodyMask = Skeleton->BuildUpperBodyMask();
        AnimTime = 0.0f;
        NativeInitializeAnimation();
    }

    void UAnimInstance::NativeInitializeAnimation() {}

    void UAnimInstance::SetBlendParamNames(const std::string& InAxisX, const std::string& InAxisY) {
        BlendParamX = InAxisX;
        BlendParamY = InAxisY;
    }

    void UAnimInstance::SetFloat(const std::string& InName, float InValue) {
        FloatParams[InName] = InValue;
    }

    float UAnimInstance::GetFloat(const std::string& InName, float InDefault) const {
        auto it = FloatParams.find(InName);
        return it != FloatParams.end() ? it->second : InDefault;
    }

    void UAnimInstance::SetBool(const std::string& InName, bool bInValue) {
        BoolParams[InName] = bInValue;
    }

    bool UAnimInstance::GetBool(const std::string& InName, bool bDefault) const {
        auto it = BoolParams.find(InName);
        return it != BoolParams.end() ? it->second : bDefault;
    }

    void UAnimInstance::NativeUpdateAnimation(float InDeltaSeconds) {
        AnimTime += InDeltaSeconds;
        StateMachine.Update(InDeltaSeconds, *this);
    }

    void UAnimInstance::Evaluate(FPose& OutPose) {
        if (!Skeleton) {
            OutPose = {};
            return;
        }

        if (OverrideSequence) {
            FAnimRuntime::SampleSequence(*OverrideSequence, *Skeleton, AnimTime, bOverrideLoop, OutPose);
        } else {
            StateMachine.Evaluate(*Skeleton, *this, OutPose);
        }

        if (UpperBodySequence && !UpperBodyMask.empty()) {
            FPose upper;
            FAnimRuntime::SampleSequence(*UpperBodySequence, *Skeleton, AnimTime, true, upper);
            FPose layered;
            FAnimRuntime::LayeredBlend(OutPose, upper, UpperBodyMask, layered);
            OutPose = std::move(layered);
        }
    }

    void UAnimInstance::ApplyRepState(const FAnimRepState& InState) {
        SetFloat("Speed", InState.Speed);
        SetFloat("Direction", InState.Direction);
        SetFloat("AimPitch", InState.AimPitch);
        SetBool("bIsFalling", InState.IsInAir());
        SetBool("bIsInAir", InState.IsInAir());
        SetBool("bIsCrouched", InState.IsCrouched());
    }

    FAnimRepState UAnimInstance::BuildRepState() const {
        FAnimRepState state;
        state.Speed = GetFloat("Speed");
        state.Direction = GetFloat("Direction");
        state.AimPitch = GetFloat("AimPitch");
        state.SetFlag(FAnimRepState::FlagInAir, GetBool("bIsFalling") || GetBool("bIsInAir"));
        state.SetFlag(FAnimRepState::FlagCrouched, GetBool("bIsCrouched"));
        return state;
    }

    void UAnimInstance::ResetPoseState() {
        AnimTime = 0.0f;
        ClearOverrideSequence();
        SetUpperBodySequence(nullptr);
        SetBool("bIsDead", false);
        SetBool("bIsFalling", false);
        SetFloat("Speed", 0.0f);
        SetFloat("Direction", 0.0f);
        SetFloat("VerticalSpeed", 0.0f);
        StateMachine.ResetToDefault();
    }

    void UAnimInstance::SetOverrideSequence(const TRef<UAnimSequence>& InSeq, bool bLoop) {
        OverrideSequence = InSeq;
        bOverrideLoop = bLoop;
        AnimTime = 0.0f;
        if (OverrideSequence && Skeleton)
            OverrideSequence->LinkSkeleton(Skeleton);
    }

    void UAnimInstance::PlayOneShotOverride(const TRef<UAnimSequence>& InSeq, bool bLoop) {
        SetOverrideSequence(InSeq, bLoop);
    }

    void UAnimInstance::ClearOverrideSequence() {
        OverrideSequence = nullptr;
    }

} // namespace Leon
