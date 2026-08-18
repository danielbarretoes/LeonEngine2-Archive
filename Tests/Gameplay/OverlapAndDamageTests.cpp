#include "Engine/UWorld.hpp"
#include "Engine/ENetTypes.hpp"
#include "Gameplay/AActor.hpp"
#include "Gameplay/AGameModeBase.hpp"
#include "Gameplay/UHealthComponent.hpp"
#include "Gameplay/UPrimitiveComponent.hpp"
#include "Gameplay/UGameplayStatics.hpp"
#include "Physics/FHitResult.hpp"

#include <doctest/doctest.h>

namespace Leon {

    namespace {
        struct FOverlapDamageFixture {
            TRef<UWorld> World;
            AGameModeBase* GM = nullptr;

            FOverlapDamageFixture() {
                World = UWorld::Create("OverlapDamageWorld");
                GM = World->SpawnActor<AGameModeBase>("GM");
                World->SetGameMode(GM);
                World->InitWorld();
                GM->HUDClass = "None";
                GM->DefaultPawnClass = "None";
                World->BeginPlay();
            }
        };
    } // namespace

    TEST_SUITE("Overlap events and ApplyDamage") {

        TEST_CASE("begin and end overlap fire when generators touch") {
            FOverlapDamageFixture f;
            AActor* a = f.World->SpawnActor<AActor>("A");
            AActor* b = f.World->SpawnActor<AActor>("B");
            a->ExecuteBeginPlay();
            b->ExecuteBeginPlay();

            auto boxA = a->AddActorComponent<UBoxComponent>("BoxA");
            auto boxB = b->AddActorComponent<UBoxComponent>("BoxB");
            boxA->SetBoxExtent({0.5f, 0.5f, 0.5f});
            boxB->SetBoxExtent({0.5f, 0.5f, 0.5f});
            boxA->SetGenerateOverlapEvents(true);
            boxA->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            boxB->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            a->SetActorLocation({0.0f, 0.0f, 0.0f});
            b->SetActorLocation({0.4f, 0.0f, 0.0f});

            int begins = 0;
            int ends = 0;
            boxA->OnComponentBeginOverlap.push_back(
                [&](UPrimitiveComponent*, AActor* other, UPrimitiveComponent*, const FHitResult&) {
                    if (other == b)
                        ++begins;
                });
            boxA->OnComponentEndOverlap.push_back(
                [&](UPrimitiveComponent*, AActor* other, UPrimitiveComponent*, const FHitResult&) {
                    if (other == b)
                        ++ends;
                });

            f.World->UpdateComponentOverlaps();
            CHECK(begins == 1);
            CHECK(ends == 0);

            b->SetActorLocation({5.0f, 0.0f, 0.0f});
            f.World->UpdateComponentOverlaps();
            CHECK(ends == 1);
        }

        TEST_CASE("ApplyPointDamage reduces health until dead") {
            FOverlapDamageFixture f;
            AActor* victim = f.World->SpawnActor<AActor>("Victim");
            victim->ExecuteBeginPlay();
            auto health = victim->AddActorComponent<UHealthComponent>("Health");
            health->SetMaxHealth(50.0f);
            health->ResetHealth();

            FHitResult hit;
            hit.ImpactPoint = victim->GetActorLocation();
            hit.ImpactNormal = {0.0f, 1.0f, 0.0f};
            CHECK(UGameplayStatics::ApplyPointDamage(f.World.get(), victim, 20.0f, {0.0f, 0.0f, 1.0f}, hit, nullptr,
                                                     nullptr));
            CHECK(health->GetHealth() == doctest::Approx(30.0f));
            CHECK(UGameplayStatics::ApplyPointDamage(f.World.get(), victim, 40.0f, {0.0f, 0.0f, 1.0f}, hit, nullptr,
                                                     nullptr));
            CHECK(health->IsDead());
        }

        TEST_CASE("ApplyRadialDamage hits actors in radius with falloff") {
            FOverlapDamageFixture f;
            AActor* nearA = f.World->SpawnActor<AActor>("Near");
            AActor* farA = f.World->SpawnActor<AActor>("Far");
            nearA->ExecuteBeginPlay();
            farA->ExecuteBeginPlay();
            auto hNear = nearA->AddActorComponent<UHealthComponent>("HN");
            auto hFar = farA->AddActorComponent<UHealthComponent>("HF");
            hNear->ResetHealth();
            hFar->ResetHealth();
            nearA->SetActorLocation({0.0f, 0.0f, 0.0f});
            farA->SetActorLocation({8.0f, 0.0f, 0.0f});

            const int32_t n =
                UGameplayStatics::ApplyRadialDamage(f.World.get(), 50.0f, {0.0f, 0.0f, 0.0f}, 5.0f, nullptr, nullptr);
            CHECK(n == 1);
            CHECK(hNear->GetHealth() < 100.0f);
            CHECK(hFar->GetHealth() == doctest::Approx(100.0f));
        }
    }

} // namespace Leon
