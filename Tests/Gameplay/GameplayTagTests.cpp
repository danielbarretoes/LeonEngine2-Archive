#include "Gameplay/FGameplayTag.hpp"

#include "doctest/doctest.h"

namespace Leon {

    TEST_SUITE("GameplayTag") {

        TEST_CASE("RequestTag is idempotent and names round-trip") {
            const FGameplayTag dead = FGameplayTagRegistry::RequestTag("State.Dead");
            const FGameplayTag deadAgain = FGameplayTagRegistry::RequestTag("State.Dead");
            CHECK(dead.IsValid());
            CHECK(dead == deadAgain);
            CHECK(std::string(FGameplayTagRegistry::GetTagName(dead)) == "State.Dead");
        }

        TEST_CASE("FGameplayTagContainer HasTag AddTag RemoveTag") {
            const FGameplayTag rifle = FGameplayTagRegistry::RequestTag("Weapon.Rifle");
            const FGameplayTag shotgun = FGameplayTagRegistry::RequestTag("Weapon.Shotgun");

            FGameplayTagContainer container;
            CHECK(container.IsEmpty());
            CHECK_FALSE(container.HasTag(rifle));

            container.AddTag(rifle);
            CHECK(container.HasTag(rifle));
            CHECK_FALSE(container.HasTag(shotgun));

            container.AddTag(rifle);
            CHECK(container.Tags.size() == 1);

            container.AddTag(shotgun);
            FGameplayTagContainer other;
            other.AddTag(shotgun);
            CHECK(container.HasAny(other));
            CHECK(other.HasAny(container));
            CHECK(container.HasAll(other));
            CHECK_FALSE(other.HasAll(container));

            const FGameplayTag sniper = FGameplayTagRegistry::RequestTag("Weapon.Sniper");
            FGameplayTagContainer disjoint;
            disjoint.AddTag(sniper);
            CHECK_FALSE(container.HasAny(disjoint));
            CHECK_FALSE(disjoint.HasAny(container));

            container.RemoveTag(rifle);
            CHECK_FALSE(container.HasTag(rifle));
            CHECK(container.HasTag(shotgun));
        }
    }

} // namespace Leon
