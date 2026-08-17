#pragma once

#include "Gameplay/UAnimInstance.hpp"
#include "Assets/UBlendSpace.hpp"

namespace Leon {

    /**
     * Tournament locomotion graph: 2D blend (speed x direction) + airborne clips + death.
     * Death is driven by UHealthComponent via the bIsDead parameter.
     */
    class ULeonTournamentAnimInstance : public UAnimInstance {
    public:
        ULeonTournamentAnimInstance(const std::string& InName = "LeonTournamentAnimInstance");

        void NativeInitializeAnimation() override;
        void NativeUpdateAnimation(float InDeltaSeconds) override;

        TRef<UBlendSpace> GetLocomotionBlendSpace() const { return LocomotionBlend; }

        static TRef<UBlendSpace> BuildLocomotionBlendSpace(const TRef<USkeleton>& InSkeleton);
        static bool LocomotionBlendCoversEightDirections(const UBlendSpace& InBlend);

        void PlayDeathMontage();
        TRef<UAnimSequence> GetDeathSequence() const { return DeathSequence; }

    private:
        TRef<UAnimSequence> LoadLinked(const std::string& InPath);
        TRef<UBlendSpace> LocomotionBlend;
        TRef<UAnimSequence> DeathSequence;
        TRef<UAnimSequence> IdleSequence;
        bool bGraphBuilt = false;
    };

} // namespace Leon
