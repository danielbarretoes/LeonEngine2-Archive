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
            CHECK_FALSE(other.HasAny(container));

            container.RemoveTag(rifle);
            CHECK_FALSE(container.HasTag(rifle));
            CHECK(container.HasTag(shotgun));
        }
    }

} // namespace Leon
