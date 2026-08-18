#include <doctest/doctest.h>

#include "Engine/UWorld.hpp"
#include "Gameplay/ACharacter.hpp"
#include "Gameplay/AAIController.hpp"
#include "Gameplay/UPrimitiveComponent.hpp"
#include "AI/UAIPerceptionComponent.hpp"

TEST_SUITE("AI Perception") {
    TEST_CASE("actor in FOV with clear trace is perceived") {
        auto world = Leon::UWorld::Create("PercWorld");
        auto* self = world->SpawnActor<Leon::ACharacter>("Self");
        auto* other = world->SpawnActor<Leon::ACharacter>("Other");
        auto* ai = world->SpawnActor<Leon::AAIController>("AI");
        self->SetActorLocation({0.0f, 1.7f, 0.0f});
        other->SetActorLocation({0.0f, 1.7f, -8.0f});
        world->AddAIController(ai);
        ai->Possess(self);

        auto perception = ai->GetPerceptionComponent();
        REQUIRE(perception);
        perception->UpdatePerception();
        CHECK(ai->HasLineOfSightTo(*other));
        CHECK(perception->GetCurrentTarget() == other);
        bool bFound = false;
        for (Leon::AActor* actor : ai->GetPerceivedActors()) {
            if (actor == other)
                bFound = true;
        }
        CHECK(bFound);
    }

    TEST_CASE("wall blocks line of sight") {
        auto world = Leon::UWorld::Create("PercWall");
        auto* wall = world->SpawnActor<Leon::AActor>("Wall");
        wall->SetActorLocation({0.0f, 1.5f, -4.0f});
        wall->SetActorScale({6.0f, 3.0f, 0.8f});
        auto box = wall->AddActorComponent<Leon::UBoxComponent>("Box");
        box->SetBoxExtent({0.5f, 0.5f, 0.5f});
        box->SetCollisionObjectType(Leon::ECollisionChannel::WorldStatic);

        auto* self = world->SpawnActor<Leon::ACharacter>("Self");
        auto* other = world->SpawnActor<Leon::ACharacter>("Other");
        auto* ai = world->SpawnActor<Leon::AAIController>("AI");
        self->SetActorLocation({0.0f, 1.7f, 0.0f});
        other->SetActorLocation({0.0f, 1.7f, -8.0f});
        world->AddAIController(ai);
        ai->Possess(self);

        CHECK_FALSE(ai->HasLineOfSightTo(*other));
        auto perception = ai->GetPerceptionComponent();
        REQUIRE(perception);
        CHECK_FALSE(perception->HasLineOfSight(*other));
    }
}
