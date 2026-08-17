#include <doctest/doctest.h>

#include "Gameplay/UHealthComponent.hpp"
#include "Gameplay/FDamageInfo.hpp"
#include "Engine/UWorld.hpp"
#include "Engine/Components.hpp"
#include "Gameplay/AActor.hpp"

#include <glm/glm.hpp>

namespace Leon {

    TEST_SUITE("UHealthComponent") {

        TEST_CASE("full health, damage, heal, death, ignore post-death") {
            UHealthComponent health;
            CHECK(health.GetHealth() == doctest::Approx(100.0f));
            CHECK(health.GetMaxHealth() == doctest::Approx(100.0f));
            CHECK_FALSE(health.IsDead());

            int changed = 0, damaged = 0, healed = 0, deaths = 0;
            health.OnHealthChanged.push_back([&](float, float, float) { ++changed; });
            health.OnDamage.push_back([&](const FDamageInfo&) { ++damaged; });
            health.OnHealed.push_back([&](float, float) { ++healed; });
            health.OnDeath.push_back([&](const FDamageInfo&) { ++deaths; });

            health.ApplyDamage(40.0f);
            CHECK(health.GetHealth() == doctest::Approx(60.0f));
            CHECK(damaged == 1);

            health.Heal(15.0f);
            CHECK(health.GetHealth() == doctest::Approx(75.0f));
            CHECK(healed == 1);

            health.ApplyDamage(75.0f);
            CHECK(health.IsDead());
            CHECK(health.GetHealth() == doctest::Approx(0.0f));
            CHECK(deaths == 1);

            health.ApplyDamage(10.0f);
            health.Heal(50.0f);
            CHECK(health.IsDead());
            CHECK(health.GetHealth() == doctest::Approx(0.0f));
            CHECK(deaths == 1);
            CHECK(healed == 1);

            health.ResetHealth();
            CHECK_FALSE(health.IsDead());
            CHECK(health.GetHealth() == doctest::Approx(100.0f));
        }

        TEST_CASE("SetHealth clamps and zero health is dead") {
            UHealthComponent health;
            health.SetHealth(250.0f);
            CHECK(health.GetHealth() == doctest::Approx(100.0f));
            health.SetHealth(0.0f);
            CHECK(health.IsDead());
        }
    }

    TEST_SUITE("UWorld line trace") {

        TEST_CASE("hits blocking box and ignores origin actor") {
            auto world = UWorld::Create("TraceWorld");
            auto* wall = world->SpawnActor<AActor>("Wall");
            wall->SetActorLocation({0.0f, 1.0f, 5.0f});
            wall->AddComponent<FBoxCollisionComponent>(glm::vec3(-0.5f), glm::vec3(0.5f), true);
            auto* self = world->SpawnActor<AActor>("Self");
            self->SetActorLocation({0.0f, 1.0f, 0.0f});
            self->AddComponent<FBoxCollisionComponent>(glm::vec3(-0.5f), glm::vec3(0.5f), true);

            UWorld::FHitResult hit;
            CHECK(world->LineTrace({0, 1, 0}, {0, 1, 20}, self, hit));
            CHECK(hit.Actor == wall);
            CHECK_FALSE(world->LineTrace({0, 1, 0}, {0, 1, 2}, self, hit));
        }

        TEST_CASE("LineTraceByChannel respects collider channels") {
            auto world = UWorld::Create("ChannelWorld");
            auto* wall = world->SpawnActor<AActor>("Wall");
            wall->SetActorLocation({0.0f, 1.0f, 5.0f});
            wall->AddComponent<FBoxCollisionComponent>(glm::vec3(-0.5f), glm::vec3(0.5f), true,
                                                       ECollisionChannel::WorldStatic);
            auto* pawnBox = world->SpawnActor<AActor>("PawnBox");
            pawnBox->SetActorLocation({0.0f, 1.0f, 8.0f});
            pawnBox->AddComponent<FBoxCollisionComponent>(glm::vec3(-0.5f), glm::vec3(0.5f), true,
                                                          ECollisionChannel::Pawn);

            UWorld::FHitResult hit;
            CHECK(world->LineTraceByChannel({0, 1, 0}, {0, 1, 20}, ECollisionChannel::WorldStatic, nullptr, hit));
            CHECK(hit.Actor == wall);
            CHECK(world->LineTraceByChannel({0, 1, 6.5f}, {0, 1, 20}, ECollisionChannel::Pawn, nullptr, hit));
            CHECK(hit.Actor == pawnBox);
            CHECK(world->LineTraceByChannel({0, 1, 0}, {0, 1, 20}, ECollisionChannel::Visibility, nullptr, hit));
            CHECK(hit.Actor == wall);
        }
    }

} // namespace Leon
