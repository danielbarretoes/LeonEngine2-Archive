#include "Gameplay/UHealthComponent.hpp"

#include <algorithm>

namespace Leon {

    UHealthComponent::UHealthComponent(const std::string& InName) : UActorComponent(InName) {}

    void UHealthComponent::SetMaxHealth(float InMax) {
        MaxHealth = std::max(InMax, 0.0f);
        if (Health > MaxHealth)
            SetHealth(MaxHealth);
    }

    void UHealthComponent::SetHealth(float InHealth) {
        const float old = Health;
        Health = std::clamp(InHealth, 0.0f, MaxHealth);
        bIsDead = Health <= 0.0f;
        BroadcastHealthChanged(old);
    }

    void UHealthComponent::ResetHealth() {
        const float old = Health;
        Health = MaxHealth;
        bIsDead = false;
        BroadcastHealthChanged(old);
    }

    void UHealthComponent::ApplyDamage(float InAmount) {
        FDamageInfo info;
        info.DamageAmount = InAmount;
        info.HitActor = GetOwner();
        ApplyDamage(info);
    }

    void UHealthComponent::ApplyDamage(const FDamageInfo& InInfo) {
        if (bIsDead)
            return;

        const float amount = std::max(InInfo.DamageAmount, 0.0f);
        if (amount <= 0.0f)
            return;

        FDamageInfo info = InInfo;
        info.HitActor = info.HitActor ? info.HitActor : GetOwner();

        const float old = Health;
        Health = std::max(0.0f, Health - amount);
        for (auto& cb : OnDamage) {
            if (cb)
                cb(info);
        }
        BroadcastHealthChanged(old);

        if (Health <= 0.0f)
            BecomeDead(info);
    }

    void UHealthComponent::Heal(float InAmount) {
        if (bIsDead)
            return;
        const float amount = std::max(InAmount, 0.0f);
        if (amount <= 0.0f)
            return;
        const float old = Health;
        Health = std::min(MaxHealth, Health + amount);
        for (auto& cb : OnHealed) {
            if (cb)
                cb(amount, Health);
        }
        BroadcastHealthChanged(old);
    }

    void UHealthComponent::BroadcastHealthChanged(float InOld) {
        if (InOld == Health)
            return;
        for (auto& cb : OnHealthChanged) {
            if (cb)
                cb(InOld, Health, MaxHealth);
        }
    }

    void UHealthComponent::BecomeDead(const FDamageInfo& InInfo) {
        if (bIsDead)
            return;
        bIsDead = true;
        Health = 0.0f;
        for (auto& cb : OnDeath) {
            if (cb)
                cb(InInfo);
        }
    }

} // namespace Leon
