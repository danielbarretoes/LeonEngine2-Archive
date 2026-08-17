#pragma once

#include "Gameplay/UObject.hpp"
#include "Assets/FAnimTypes.hpp"
#include "Assets/UAnimSequence.hpp"
#include "Assets/UBlendSpace.hpp"
#include "Assets/USkeleton.hpp"
#include "Gameplay/FAnimStateMachine.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace Leon {

    /**
     * Generic animation evaluation host. Named float/bool parameters feed the state machine
     * and blend spaces. Game subclasses set parameters and build the graph.
     */
    class UAnimInstance : public UObject {
    public:
        UAnimInstance(const std::string& InName = "AnimInstance");
        ~UAnimInstance() override = default;

        void Initialize(const TRef<USkeleton>& InSkeleton);

        virtual void NativeInitializeAnimation();
        virtual void NativeUpdateAnimation(float InDeltaSeconds);
        virtual void Evaluate(FPose& OutPose);

        void ApplyRepState(const FAnimRepState& InState);
        FAnimRepState BuildRepState() const;

        void SetFloat(const std::string& InName, float InValue);
        float GetFloat(const std::string& InName, float InDefault = 0.0f) const;
        void SetBool(const std::string& InName, bool bInValue);
        bool GetBool(const std::string& InName, bool bDefault = false) const;

        TRef<USkeleton> GetSkeleton() const { return Skeleton; }
        FAnimStateMachine& GetStateMachine() { return StateMachine; }
        const FAnimStateMachine& GetStateMachine() const { return StateMachine; }

        void SetBlendParamNames(const std::string& InAxisX, const std::string& InAxisY);
        const std::string& GetBlendParamX() const { return BlendParamX; }
        const std::string& GetBlendParamY() const { return BlendParamY; }

        void SetUpperBodySequence(const TRef<UAnimSequence>& InSeq) { UpperBodySequence = InSeq; }
        TRef<UAnimSequence> GetUpperBodySequence() const { return UpperBodySequence; }

        void SetOverrideSequence(const TRef<UAnimSequence>& InSeq, bool bLoop = true);
        void ClearOverrideSequence();

        float GetAnimTime() const { return AnimTime; }

    protected:
        TRef<USkeleton> Skeleton;
        FAnimStateMachine StateMachine;
        TRef<UAnimSequence> UpperBodySequence;
        TRef<UAnimSequence> OverrideSequence;
        bool bOverrideLoop = true;
        float AnimTime = 0.0f;
        std::vector<float> UpperBodyMask;
        std::string BlendParamX = "Speed";
        std::string BlendParamY = "Direction";
        std::unordered_map<std::string, float> FloatParams;
        std::unordered_map<std::string, bool> BoolParams;
    };

} // namespace Leon
