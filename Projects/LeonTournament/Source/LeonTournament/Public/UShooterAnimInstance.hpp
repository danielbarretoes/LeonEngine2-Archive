#pragma once

#include "Gameplay/UAnimInstance.hpp"
#include "Assets/UBlendSpace.hpp"

namespace Leon {

    /**
     * Tournament locomotion graph: 2D blend (speed x direction) + airborne clips + death.
     * Death is driven by UHealthComponent via the bIsDead parameter.
     */
    class UShooterAnimInstance : public UAnimInstance {
    public:
        UShooterAnimInstance(const std::string& InName = "ShooterAnimInstance");

        void NativeInitializeAnimation() override;
        void NativeUpdateAnimation(float InDeltaSeconds) override;

        TRef<UBlendSpace> GetLocomotionBlendSpace() const { return LocomotionBlend; }

        static TRef<UBlendSpace> BuildLocomotionBlendSpace(const TRef<USkeleton>& InSkeleton);

    private:
        TRef<UAnimSequence> LoadLinked(const std::string& InPath);
        TRef<UBlendSpace> LocomotionBlend;
        TRef<UAnimSequence> DeathSequence;
        TRef<UAnimSequence> IdleSequence;
        bool bGraphBuilt = false;
    };

} // namespace Leon
