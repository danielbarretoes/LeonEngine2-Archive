#include <doctest/doctest.h>

#include "Engine/UWorld.hpp"
#include "Gameplay/ACharacter.hpp"
#include "Gameplay/ABlockingVolume.hpp"
#include "Gameplay/APhysicsVolume.hpp"
#include "Gameplay/UPrimitiveComponent.hpp"
#include "Gameplay/UCharacterMovementComponent.hpp"
#include "Gameplay/USpringArmComponent.hpp"
#include "Gameplay/APawn.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Gameplay/AAIController.hpp"
#include "AI/UBlackboardData.hpp"
#include "AI/UBlackboardComponent.hpp"
#include "AI/UBehaviorTree.hpp"
#include "AI/UBehaviorTreeComponent.hpp"
#include "Assets/UPhysicsAsset.hpp"
#include "Physics/IPhysicsScene.hpp"

TEST_SUITE("Physics / Collision") {
    TEST_CASE("UBoxComponent blocks Visibility traces") {
        auto world = Leon::UWorld::Create("PhysBox");
        auto* wall = world->SpawnActor<Leon::AActor>("Wall");
        wall->SetActorLocation({0.0f, 1.0f, 10.0f});
        auto box = wall->AddActorComponent<Leon::UBoxComponent>("Box");
        box->SetBoxExtent({0.5f, 0.5f, 0.5f});
        box->SetCollisionObjectType(Leon::ECollisionChannel::WorldStatic);

        Leon::FHitResult hit;
        CHECK(world->LineTraceSingleByChannel({0, 1, 0}, {0, 1, 20}, Leon::ECollisionChannel::Visibility, nullptr, hit));
        CHECK(hit.bBlockingHit);
        CHECK(hit.Actor == wall);
        CHECK(hit.Component != nullptr);
    }

    TEST_CASE("USphereComponent and UCapsuleComponent traces") {
        auto world = Leon::UWorld::Create("PhysShapes");
        auto* sphereActor = world->SpawnActor<Leon::AActor>("Sphere");
        sphereActor->SetActorLocation({0.0f, 1.0f, 8.0f});
        auto sphere = sphereActor->AddActorComponent<Leon::USphereComponent>("Sphere");
        sphere->SetSphereRadius(0.5f);

        Leon::FHitResult hit;
        CHECK(world->LineTraceSingleByChannel({0, 1, 0}, {0, 1, 20}, Leon::ECollisionChannel::Visibility, nullptr, hit));
        CHECK(hit.Actor == sphereActor);

        auto* capActor = world->SpawnActor<Leon::AActor>("Capsule");
        capActor->SetActorLocation({5.0f, 1.0f, 8.0f});
        auto cap = capActor->AddActorComponent<Leon::UCapsuleComponent>("Cap");
        cap->SetCapsuleSize(0.4f, 0.9f);
        CHECK(world->LineTraceSingleByChannel({5, 1, 0}, {5, 1, 20}, Leon::ECollisionChannel::Visibility, nullptr, hit));
        CHECK(hit.Actor == capActor);
    }

    TEST_CASE("Collision responses Ignore vs Block") {
        auto world = Leon::UWorld::Create("PhysResp");
        auto* wall = world->SpawnActor<Leon::AActor>("Wall");
        wall->SetActorLocation({0.0f, 1.0f, 10.0f});
        auto box = wall->AddActorComponent<Leon::UBoxComponent>("Box");
        box->SetBoxExtent({0.5f, 0.5f, 0.5f});
        box->SetCollisionResponseToChannel(Leon::ECollisionChannel::Visibility, Leon::ECollisionResponse::Ignore);
        Leon::FHitResult hit;
        CHECK_FALSE(world->LineTraceSingleByChannel({0, 1, 0}, {0, 1, 20}, Leon::ECollisionChannel::Visibility, nullptr,
                                                    hit));
        box->SetCollisionResponseToChannel(Leon::ECollisionChannel::Visibility, Leon::ECollisionResponse::Block);
        CHECK(world->LineTraceSingleByChannel({0, 1, 0}, {0, 1, 20}, Leon::ECollisionChannel::Visibility, nullptr, hit));
    }

    TEST_CASE("SimulatePhysics dynamic body falls") {
        auto world = Leon::UWorld::Create("PhysDyn");
        auto* scene = world->GetPhysicsScene();
        REQUIRE(scene);
        Leon::FPhysicsBodyCreateInfo info;
        info.Motion = Leon::EPhysicsMotionType::Dynamic;
        info.bSimulatePhysics = true;
        info.bEnableGravity = true;
        info.Location = {0.0f, 5.0f, 0.0f};
        auto* body = scene->CreateRigidBody(info);
        REQUIRE(body);
        CHECK(body->IsSimulating());
        glm::vec3 loc;
        glm::quat rot;
        scene->Tick(0.5f);
        body->GetTransform(loc, rot);
        CHECK(loc.y < 5.0f);
        scene->DestroyRigidBody(body);
    }

    TEST_CASE("Sweep and overlap by channel") {
        auto world = Leon::UWorld::Create("PhysSweep");
        auto* wall = world->SpawnActor<Leon::AActor>("Wall");
        wall->SetActorLocation({0.0f, 1.0f, 4.0f});
        auto box = wall->AddActorComponent<Leon::UBoxComponent>("Box");
        box->SetBoxExtent({0.5f, 1.0f, 0.5f});
        Leon::FHitResult hit;
        CHECK(world->SweepSingleByChannel({0, 1, 0}, {0, 1, 8}, 0.2f, Leon::ECollisionChannel::WorldStatic, nullptr, hit));
        CHECK(world->OverlapAnyTestByChannel({0, 1, 4}, {0.6f, 0.6f, 0.6f}, Leon::ECollisionChannel::WorldStatic,
                                             nullptr));
    }

    TEST_CASE("ABlockingVolume and APhysicsVolume") {
        auto world = Leon::UWorld::Create("Volumes");
        auto* block = world->SpawnActor<Leon::ABlockingVolume>("Block");
        block->SetActorLocation({0.0f, 1.0f, 6.0f});
        block->SetActorScale({2.0f, 2.0f, 2.0f});
        block->PostInitializeComponents();
        Leon::FHitResult hit;
        CHECK(world->LineTraceByChannel({0, 1, 0}, {0, 1, 12}, Leon::ECollisionChannel::Visibility, nullptr, hit));
        auto* vol = world->SpawnActor<Leon::APhysicsVolume>("Water");
        vol->PostInitializeComponents();
        vol->GravityScale = 0.3f;
        CHECK(vol->GravityScale == doctest::Approx(0.3f));
    }
}

TEST_SUITE("CharacterMovement") {
    TEST_CASE("Jump math and IsFalling") {
        auto world = Leon::UWorld::Create("JumpWorld");
        auto* floor = world->SpawnActor<Leon::AActor>("Floor");
        floor->SetActorLocation({0.0f, -0.25f, 0.0f});
        floor->SetActorScale({20.0f, 0.5f, 20.0f});
        auto box = floor->AddActorComponent<Leon::UBoxComponent>("Box");
        box->SetBoxExtent({0.5f, 0.5f, 0.5f});

        auto* ch = world->SpawnActor<Leon::ACharacter>("Jumper");
        ch->SetFloorZ(0.0f);
        ch->SetActorLocation({0.0f, 1.7f, 0.0f});
        REQUIRE(ch->GetCharacterMovement());
        auto move = ch->GetCharacterMovement();
        const float v0 = move->GetJumpZVelocity();
        const float g = move->GetGravityZ() * move->GetGravityScale();
        CHECK(ch->IsMovingOnGround());
        CHECK_FALSE(ch->IsFalling());
        CHECK(ch->CanJump());
        ch->Jump();
        const float dt = 0.05f;
        move->PerformMovement(dt);
        float v = v0 - g * dt;
        CHECK(ch->IsFalling());
        CHECK(move->GetMovementMode() == Leon::EMovementMode::Falling);
        CHECK(move->GetVelocity().y == doctest::Approx(v).epsilon(0.2f));

        for (int i = 0; i < 80; ++i)
            move->PerformMovement(0.05f);
        CHECK(ch->IsMovingOnGround());
        CHECK_FALSE(ch->IsFalling());
        CHECK(move->GetMovementMode() == Leon::EMovementMode::Walking);
    }

    TEST_CASE("ResolvePenetration pushes capsule out of a blocking box") {
        auto world = Leon::UWorld::Create("PenWorld");
        auto* wall = world->SpawnActor<Leon::AActor>("Wall");
        wall->SetActorLocation({0.0f, 1.0f, 0.0f});
        auto box = wall->AddActorComponent<Leon::UBoxComponent>("Box");
        box->SetBoxExtent({0.25f, 1.0f, 0.25f});
        box->SetCollisionObjectType(Leon::ECollisionChannel::WorldStatic);

        auto* ch = world->SpawnActor<Leon::ACharacter>("Stuck");
        ch->SetActorLocation({0.0f, 1.0f, 0.0f});
        auto move = ch->GetCharacterMovement();
        REQUIRE(move);

        glm::vec3 minB, maxB;
        ch->GetCapsuleAABB(minB, maxB);
        const glm::vec3 half = (maxB - minB) * 0.5f;
        CHECK(world->OverlapAnyTestByChannel((minB + maxB) * 0.5f, half, Leon::ECollisionChannel::WorldStatic, ch));

        CHECK(move->ResolvePenetration());
        ch->GetCapsuleAABB(minB, maxB);
        CHECK_FALSE(world->OverlapAnyTestByChannel((minB + maxB) * 0.5f, (maxB - minB) * 0.5f,
                                                   Leon::ECollisionChannel::WorldStatic, ch));
    }

    TEST_CASE("SimulatedProxy does not write transform from movement or physics") {
        auto world = Leon::UWorld::Create("ProxyWorld");
        auto* ch = world->SpawnActor<Leon::ACharacter>("ProxyChar");
        ch->SetActorLocation({1.0f, 1.7f, 2.0f});
        const glm::vec3 loc = ch->GetActorLocation();
        ch->SetLocalRole(Leon::ENetRole::SimulatedProxy);

        auto move = ch->GetCharacterMovement();
        REQUIRE(move);
        move->AddInputVector({20.0f, 0.0f, 0.0f});
        move->PerformMovement(0.05f);
        CHECK(ch->GetActorLocation().x == doctest::Approx(loc.x));
        CHECK(ch->GetActorLocation().y == doctest::Approx(loc.y));

        Leon::IPhysicsScene* scene = world->GetPhysicsScene();
        REQUIRE(scene);
        Leon::FPhysicsBodyCreateInfo info;
        info.Motion = Leon::EPhysicsMotionType::Dynamic;
        info.bSimulatePhysics = true;
        info.bEnableGravity = true;
        info.Location = loc;
        info.Actor = ch;
        auto* body = scene->CreateRigidBody(info);
        REQUIRE(body);
        scene->Tick(0.5f);
        CHECK(ch->GetActorLocation().y == doctest::Approx(loc.y));
        scene->DestroyRigidBody(body);
    }

    TEST_CASE("HasAuthority and IsLocallyControlled") {
        auto world = Leon::UWorld::Create("Auth");
        auto* pc = world->SpawnActor<Leon::APlayerController>("PC");
        auto* ch = world->SpawnActor<Leon::ACharacter>("Char");
        world->AddPlayerController(pc);
        pc->Possess(ch);
        CHECK(ch->HasAuthority());
        CHECK(ch->IsLocallyControlled());
        ch->SetLocalRole(Leon::ENetRole::SimulatedProxy);
        CHECK_FALSE(ch->HasAuthority());
    }
}

TEST_SUITE("SpringArm") {
    TEST_CASE("No obstacle keeps TargetArmLength") {
        auto world = Leon::UWorld::Create("ArmFree");
        auto* ch = world->SpawnActor<Leon::ACharacter>("CamChar");
        auto arm = ch->GetSpringArm();
        REQUIRE(arm);
        arm->TargetArmLength = 3.4f;
        arm->bDoCollisionTest = true;
        arm->UpdateDesiredArmLocation({0, 1.7f, 0}, {0, 0, 1}, {1, 0, 0}, {0, 1, 0});
        CHECK(arm->GetCurrentArmLength() == doctest::Approx(3.4f).epsilon(0.05f));
    }

    TEST_CASE("Obstacle retracts arm") {
        auto world = Leon::UWorld::Create("ArmHit");
        auto* wall = world->SpawnActor<Leon::AActor>("Wall");
        wall->SetActorLocation({0.0f, 1.7f, -1.5f});
        auto box = wall->AddActorComponent<Leon::UBoxComponent>("Box");
        box->SetBoxExtent({0.5f, 1.0f, 0.2f});
        auto* ch = world->SpawnActor<Leon::ACharacter>("CamChar");
        ch->SetActorLocation({0.0f, 1.7f, 0.0f});
        auto arm = ch->GetSpringArm();
        arm->TargetArmLength = 3.4f;
        arm->ProbeSize = 0.18f;
        arm->bDoCollisionTest = true;
        arm->UpdateDesiredArmLocation({0, 1.7f, 0}, {0, 0, 1}, {1, 0, 0}, {0, 1, 0});
        CHECK(arm->GetCurrentArmLength() < 3.0f);
    }
}

TEST_SUITE("AI BehaviorTree / Blackboard") {
    TEST_CASE("Blackboard asset vs runtime values") {
        auto data = Leon::MakeRef<Leon::UBlackboardData>("BB");
        data->AddKey({"HasAmmo", Leon::EBlackboardKeyType::Bool, true});
        data->AddKey({"TargetLocation", Leon::EBlackboardKeyType::Vector, glm::vec3(1, 2, 3)});
        auto comp = Leon::MakeRef<Leon::UBlackboardComponent>("BBC");
        comp->InitializeFrom(data);
        CHECK(comp->GetValueAsBool("HasAmmo"));
        comp->SetValueAsBool("HasAmmo", false);
        CHECK_FALSE(comp->GetValueAsBool("HasAmmo"));
        CHECK(data->FindKey("HasAmmo") != nullptr);
        CHECK(std::get<bool>(data->FindKey("HasAmmo")->DefaultValue) == true);
    }

    TEST_CASE("Selector Sequence Decorator Service") {
        auto world = Leon::UWorld::Create("BTWorld");
        auto* ai = world->SpawnActor<Leon::AAIController>("AI");
        auto data = Leon::MakeRef<Leon::UBlackboardData>("BB");
        data->AddKey({"Go", Leon::EBlackboardKeyType::Bool, false});
        ai->UseBlackboard(data);

        int services = 0;
        auto svc = Leon::MakeRef<Leon::UBTService_Native>("Svc", [&](Leon::UBehaviorTreeComponent&, float) { ++services; });
        svc->Interval = 0.0f;

        auto seq = Leon::MakeRef<Leon::UBTComposite_Sequence>("Seq");
        seq->Decorators.push_back(Leon::MakeRef<Leon::UBTDecorator_Blackboard>("Go", true));
        seq->AddChild(Leon::MakeRef<Leon::UBTTask_Wait>(0.0f));

        auto fallback = Leon::MakeRef<Leon::UBTTask_Wait>(0.0f);
        auto root = Leon::MakeRef<Leon::UBTComposite_Selector>("Root");
        root->Services.push_back(svc);
        root->AddChild(seq);
        root->AddChild(fallback);

        auto tree = Leon::MakeRef<Leon::UBehaviorTree>("Tree");
        tree->SetRoot(root);
        tree->SetBlackboardAsset(data);
        ai->RunBehaviorTree(tree);
        ai->GetBrainComponent()->Tick(0.1f);
        CHECK(services >= 1);
        ai->GetBlackboardComponent()->SetValueAsBool("Go", true);
        ai->GetBrainComponent()->Tick(0.1f);
    }

    TEST_CASE("UPhysicsAsset roundtrip") {
        Leon::UPhysicsAsset asset("PA");
        Leon::FPhysicsAssetBody body;
        body.BoneName = "spine";
        body.Shape = Leon::EPhysicsAssetBodyShape::Capsule;
        body.Radius = 0.08f;
        body.CapsuleHalfHeight = 0.2f;
        asset.AddBody(body);
        CHECK(asset.GetBodies().size() == 1);
    }

    TEST_CASE("EnableRagdoll falls back to capsule without PhysicsAsset") {
        auto world = Leon::UWorld::Create("RagdollWorld");
        world->InitWorld();
        auto* ch = world->SpawnActor<Leon::ACharacter>("Hero");
        REQUIRE(ch);
        world->BeginPlay();
        ch->EnableRagdoll({0.0f, 80.0f, 0.0f});
        CHECK(ch->IsRagdoll());
        REQUIRE(ch->GetCapsuleComponent());
        CHECK(ch->GetCapsuleComponent()->IsSimulatingPhysics());
        ch->StopRagdoll();
        CHECK_FALSE(ch->IsRagdoll());
        CHECK_FALSE(ch->GetCapsuleComponent()->IsSimulatingPhysics());
    }
}
