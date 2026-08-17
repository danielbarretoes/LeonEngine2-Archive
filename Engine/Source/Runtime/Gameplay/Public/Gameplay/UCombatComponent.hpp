#pragma once

#include "Gameplay/UActorComponent.hpp"

#include <algorithm>
#include <functional>
#include <vector>

namespace Leon {

    class AActor;

    /**
     * @brief Generic attack-gate: cooldown, optional target, attack request.
     * Game classes decide weapons, ammo, and fire modes.
     */
    class UCombatComponent : public UActorComponent {
    public:
        UCombatComponent(const std::string& InName = "CombatComponent");

        void Tick(float DeltaSeconds) override;

        bool CanAttack() const;
        bool Attack();

        float GetAttackCooldown() const { return AttackCooldown; }
        void SetAttackCooldown(float InSeconds) { AttackCooldown = std::max(InSeconds, 0.0f); }
        float GetCooldownRemaining() const { return CooldownRemaining; }

        AActor* GetTarget() const { return Target; }
        void SetTarget(AActor* InTarget) { Target = InTarget; }

        using FAttackEvent = std::function<void()>;
        std::vector<FAttackEvent> OnAttack;

    private:
        float AttackCooldown = 0.1f;
        float CooldownRemaining = 0.0f;
        AActor* Target = nullptr;
    };

} // namespace Leon
