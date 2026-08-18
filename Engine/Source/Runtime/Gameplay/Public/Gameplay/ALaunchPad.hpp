#pragma once

#include "Gameplay/AActor.hpp"

#include <glm/glm.hpp>
#include <unordered_map>

namespace Leon {

    class ACharacter;

    /** Launches characters with a velocity impulse when they enter the trigger radius. */
    class ALaunchPad : public AActor {
    public:
        ALaunchPad() = default;
        ALaunchPad(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "LaunchPad");

        void BeginPlay() override;
        void Tick(float DeltaSeconds) override;

        void SetLaunchVelocity(const glm::vec3& InVelocity) { LaunchVelocity = InVelocity; }
        const glm::vec3& GetLaunchVelocity() const { return LaunchVelocity; }
        void SetTriggerRadius(float InRadius) { TriggerRadius = InRadius; }
        float GetTriggerRadius() const { return TriggerRadius; }
        void SetCooldown(float InSeconds) { Cooldown = InSeconds; }

    protected:
        virtual void OnLaunched(ACharacter* InCharacter);
        virtual void BuildVisual() {}

        glm::vec3 LaunchVelocity{0.0f, 16.0f, 0.0f};
        float TriggerRadius = 1.4f;
        float Cooldown = 0.35f;
        std::unordered_map<ACharacter*, float> RecentTriggers;
    };

} // namespace Leon
