#include "Gameplay/ALaunchPad.hpp"
#include "Gameplay/ACharacter.hpp"
#include "Engine/UWorld.hpp"
#include "Engine/ENetTypes.hpp"

namespace Leon {

    ALaunchPad::ALaunchPad(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AActor(InHandle, InWorld, InName) {
        SetClass("ALaunchPad");
    }

    void ALaunchPad::BeginPlay() {
        AActor::BeginPlay();
        BuildVisual();
    }

    void ALaunchPad::OnLaunched(ACharacter* InCharacter) {
        (void)InCharacter;
    }

    void ALaunchPad::Tick(float DeltaSeconds) {
        AActor::Tick(DeltaSeconds);
        if (!World || World->GetNetMode() == ENetMode::Client)
            return;

        for (auto it = RecentTriggers.begin(); it != RecentTriggers.end();) {
            it->second -= DeltaSeconds;
            if (it->second <= 0.0f)
                it = RecentTriggers.erase(it);
            else
                ++it;
        }

        for (const auto& actorRef : World->GetAllActors()) {
            auto* ch = dynamic_cast<ACharacter*>(actorRef.get());
            if (!ch || ch->IsPendingKill())
                continue;
            if (RecentTriggers.count(ch))
                continue;
            const float dist = glm::length(ch->GetActorLocation() - GetActorLocation());
            if (dist > TriggerRadius)
                continue;
            ch->LaunchCharacter(LaunchVelocity);
            RecentTriggers[ch] = Cooldown;
            OnLaunched(ch);
        }
    }

} // namespace Leon
