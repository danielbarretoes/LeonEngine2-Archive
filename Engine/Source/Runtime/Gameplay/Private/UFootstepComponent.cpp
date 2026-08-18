#include "Gameplay/UFootstepComponent.hpp"
#include "Gameplay/ACharacter.hpp"
#include "Gameplay/UGameplayStatics.hpp"
#include "Gameplay/UHealthComponent.hpp"

#include <algorithm>

namespace Leon {

    UFootstepComponent::UFootstepComponent(const std::string& InName) : UActorComponent(InName) {}

    void UFootstepComponent::Tick(float DeltaSeconds) {
        CooldownRemaining = std::max(0.0f, CooldownRemaining - DeltaSeconds);
        if (SoundPath.empty())
            return;
        auto* character = dynamic_cast<ACharacter*>(Owner);
        if (!character || character->IsFalling())
            return;
        if (auto* health = character->FindComponentByClass<UHealthComponent>(); health && health->IsDead())
            return;
        const float speed = character->GetAnimRepState().Speed;
        if (speed < MinSpeed || CooldownRemaining > 0.0f)
            return;
        const float maxSpeed = std::max(1.0f, character->GetMoveSpeed());
        CooldownRemaining = std::clamp(0.42f * (maxSpeed / speed), 0.26f, 0.52f);
        UGameplayStatics::PlaySoundAtLocation(SoundPath, character->GetActorLocation(), Volume, AttenuationRadius);
    }

} // namespace Leon
