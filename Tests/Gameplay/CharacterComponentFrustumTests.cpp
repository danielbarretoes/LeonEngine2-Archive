#include <doctest/doctest.h>

#include "Core/FTimestep.hpp"
#include "Gameplay/AActor.hpp"
#include "Gameplay/ACharacter.hpp"
#include "Gameplay/USceneComponent.hpp"
#include "Gameplay/UClassRegistry.hpp"
#include "Renderer/FFrustumCull.hpp"
#include "UMG/UImage.hpp"
#include "Engine/UWorld.hpp"

#include <glm/gtc/matrix_transform.hpp>

namespace Leon {

    class UTestRotatorComponent : public UActorComponent {
    public:
        UTestRotatorComponent(const std::string& InName = "TestRotator") : UActorComponent(InName) {}
        void BeginPlay() override { bBeginPlay = true; }
        void Tick(float DeltaSeconds) override {
            bTick = true;
            Accum += DeltaSeconds;
        }
        void EndPlay() override { bEndPlay = true; }

        bool bBeginPlay = false;
        bool bTick = false;
        bool bEndPlay = false;
        float Accum = 0.0f;
    };

    TEST_SUITE("ACharacter / UActorComponent / Frustum / UImage") {

        TEST_CASE("ACharacter registered and clamps to floor") {
            CHECK(UClassRegistry::Get().HasClass("ACharacter"));

            auto world = UWorld::Create("CharWorld");
            auto* character = world->SpawnActor<ACharacter>("Hero");
            REQUIRE(character != nullptr);
            character->SetFloorZ(2.0f);
            character->SetEyeHeight(1.5f);
            character->SetActorLocation({10.0f, 99.0f, -3.0f});
            character->Tick(0.016f);
            CHECK(character->GetActorLocation().y == doctest::Approx(2.0f + character->GetCapsuleHalfHeight()));
            CHECK(character->GetActorLocation().x == doctest::Approx(10.0f));
        }

        TEST_CASE("ACharacter RootComponent capsule hierarchy and half-height contract") {
            auto world = UWorld::Create("RootWorld");
            auto* character = world->SpawnActor<ACharacter>("RootHero");
            REQUIRE(character != nullptr);

            auto capsule = character->GetCapsuleComponent();
            auto mesh = character->GetMesh();
            REQUIRE(capsule);
            REQUIRE(mesh);
            CHECK(character->GetRootComponent() == capsule.get());
            CHECK(mesh->GetAttachParent() == capsule.get());
            CHECK(2.0f * capsule->GetUnscaledCapsuleHalfHeight() ==
                  doctest::Approx(character->GetCapsuleHeight()).epsilon(0.01f));
            CHECK(character->GetActorLocation().y ==
                  doctest::Approx(character->GetFloorZ() + character->GetCapsuleHalfHeight()).epsilon(0.05f));
            CHECK(mesh->GetComponentLocation().y == doctest::Approx(character->GetFloorZ()).epsilon(0.05f));
        }

        TEST_CASE("USceneComponent attach yaw matches GLM Ry") {
            auto world = UWorld::Create("YawWorld");
            auto* actor = world->SpawnActor<AActor>("YawActor");
            actor->SetActorLocation({0.0f, 0.0f, 0.0f});
            actor->SetActorRotation({0.0f, 90.0f, 0.0f});
            auto parent = actor->AddActorComponent<USceneComponent>("Parent");
            auto child = actor->AddActorComponent<USceneComponent>("Child");
            REQUIRE(parent);
            REQUIRE(child);
            child->SetRelativeLocation({1.0f, 0.0f, 0.0f});
            child->SetupAttachment(parent.get());
            glm::vec3 loc = child->GetComponentLocation();
            glm::vec3 expected = glm::vec3(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0, 1, 0)) *
                                           glm::vec4(1, 0, 0, 1));
            CHECK(loc.x == doctest::Approx(expected.x).epsilon(1e-4f));
            CHECK(loc.y == doctest::Approx(expected.y).epsilon(1e-4f));
            CHECK(loc.z == doctest::Approx(expected.z).epsilon(1e-4f));
        }

        TEST_CASE("USceneComponent world matrix includes parent scale and yaw") {
            auto world = UWorld::Create("WorldMtx");
            auto* actor = world->SpawnActor<AActor>("MtxActor");
            actor->SetActorLocation({0.0f, 0.0f, 0.0f});
            actor->SetActorRotation({0.0f, 90.0f, 0.0f});
            actor->SetActorScale({2.0f, 2.0f, 2.0f});
            auto child = actor->AddActorComponent<USceneComponent>("Child");
            REQUIRE(child);
            child->SetRelativeLocation({1.0f, 0.0f, 0.0f});
            glm::vec3 loc = child->GetComponentLocation();
            CHECK(loc.x == doctest::Approx(0.0f).epsilon(1e-4f));
            CHECK(loc.y == doctest::Approx(0.0f).epsilon(1e-4f));
            CHECK(loc.z == doctest::Approx(-2.0f).epsilon(1e-4f));
            CHECK(child->GetComponentScale().x == doctest::Approx(2.0f));
            CHECK(world->FindActorByEntity(actor->GetEntityHandle()) == actor);
            glm::mat4 actorM = actor->GetActorWorldMatrix();
            glm::mat4 expected = actor->GetTransform().GetTransform();
            CHECK(actorM[3].x == doctest::Approx(expected[3].x));
            CHECK(actorM[3].y == doctest::Approx(expected[3].y));
            CHECK(actorM[3].z == doctest::Approx(expected[3].z));
        }

        TEST_CASE("UActorComponent lifecycle with ExecuteBeginPlay/Tick/EndPlay") {
            auto world = UWorld::Create("CompWorld");
            auto* actor = world->SpawnActor<AActor>("Owner");
            auto comp = actor->AddActorComponent<UTestRotatorComponent>("Rot");
            REQUIRE(comp);

            world->BeginPlay();
            // Actor was spawned before BeginPlay — ExecuteBeginPlay runs in UWorld::BeginPlay
            CHECK(comp->bBeginPlay);
            CHECK(comp->HasBegunPlay());

            world->Tick(FTimestep(0.5f));
            CHECK(comp->bTick);
            CHECK(comp->Accum == doctest::Approx(0.5f));

            world->EndPlay();
            CHECK(comp->bEndPlay);
        }

        TEST_CASE("Frustum AABB inside vs outside") {
            glm::mat4 view = glm::lookAt(glm::vec3(0, 0, 5), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
            glm::mat4 proj = glm::perspective(glm::radians(60.0f), 16.0f / 9.0f, 0.1f, 100.0f);
            FFrustumPlanes frustum = ExtractFrustumPlanes(proj * view);

            CHECK(AABBIntersectsFrustum(glm::vec3(-0.5f), glm::vec3(0.5f), frustum));
            CHECK_FALSE(AABBIntersectsFrustum(glm::vec3(1000.0f), glm::vec3(1001.0f), frustum));
        }

        TEST_CASE("UImage paints with tint fallback when no texture") {
            auto image = std::make_shared<UImage>("Img");
            image->SetTintColor({0.2f, 0.4f, 0.6f, 1.0f});
            image->SetSize({64.0f, 64.0f});
            CHECK(image->GetBrushTexture() == nullptr);
            CHECK(image->GetTintColor().g == doctest::Approx(0.4f));
        }
    }

} // namespace Leon
