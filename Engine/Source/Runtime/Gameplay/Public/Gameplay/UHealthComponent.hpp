#pragma once

#include "Gameplay/UActorComponent.hpp"
#include "Gameplay/FDamageInfo.hpp"

#include <functional>
#include <vector>

namespace Leon {

    /**
     * @brief Generic vitality. Knows only health, healing, damage, and death — no scoring or teams.
     */
    class UHealthComponent : public UActorComponent {
    public:
        UHealthComponent(const std::string& InName = "HealthComponent");

        float GetHealth() const { return Health; }
        float GetMaxHealth() const { return MaxHealth; }
        void SetMaxHealth(float InMax);
        void SetHealth(float InHealth);
        void ResetHealth();

        void ApplyDamage(float InAmount);
        void ApplyDamage(const FDamageInfo& InInfo);
        void Heal(float InAmount);
        bool IsDead() const { return bDead; }

        using FHealthChanged = std::function<void(float InOldHealth, float InNewHealth, float InMaxHealth)>;
        using FDamageEvent = std::function<void(const FDamageInfo& InInfo)>;
        using FHealEvent = std::function<void(float InAmount, float InNewHealth)>;
        using FDeathEvent = std::function<void(const FDamageInfo& InInfo)>;

        std::vector<FHealthChanged> OnHealthChanged;
        std::vector<FDamageEvent> OnDamage;
        std::vector<FHealEvent> OnHealed;
        std::vector<FDeathEvent> OnDeath;

    private:
        void BroadcastHealthChanged(float InOld);
        void BecomeDead(const FDamageInfo& InInfo);

        float MaxHealth = 100.0f;
        float Health = 100.0f;
        bool bDead = false;
    };

} // namespace Leon
