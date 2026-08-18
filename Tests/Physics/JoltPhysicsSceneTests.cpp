#include <doctest/doctest.h>

#include "Assets/UPhysicsAsset.hpp"
#include "Assets/USkeleton.hpp"
#include "Core/FTimestep.hpp"
#include "Engine/Components.hpp"
#include "Engine/UWorld.hpp"
#include "FJoltPhysicsDriver.hpp"
#include "Gameplay/ACharacter.hpp"
#include "Gameplay/UCharacterMovementComponent.hpp"
#include "Gameplay/UPrimitiveComponent.hpp"
#include "Gameplay/UProjectileMovementComponent.hpp"
#include "Physics/IPhysicsScene.hpp"
#include "Assets/UStaticMesh.hpp"

#include <cmath>
#include <vector>

namespace {

    struct FJoltPhysicsFixture {
        FJoltPhysicsFixture() { Leon::FJoltPhysicsDriver::Register(); }
        ~FJoltPhysicsFixture() { Leon::FPhysicsModule::Unregister(); }
    };

    Leon::IPhysicsBody* CreateBox(Leon::IPhysicsScene& InScene, const glm::vec3& InLocation, const glm::vec3& InHalf,
                                  bool bDynamic) {
        Leon::FPhysicsBodyCreateInfo info;
        info.Shape = Leon::EPhysicsShapeType::Box;
        info.BoxHalfExtent = InHalf;
        info.Location = InLocation;
        info.Mass = 5.0f;
        info.Restitution = 0.0f;
        info.Friction = 0.8f;
        if (bDynamic) {
            info.Motion = Leon::EPhysicsMotionType::Dynamic;
            info.bSimulatePhysics = true;
            info.bEnableGravity = true;
        } else {
            info.Motion = Leon::EPhysicsMotionType::Static;
            info.bSimulatePhysics = false;
            info.bEnableGravity = false;
            info.ObjectType = Leon::ECollisionChannel::WorldStatic;
        }
        return InScene.CreateRigidBody(info);
    }

    float RestingHeightAfter(Leon::IPhysicsScene& InScene, Leon::IPhysicsBody& InBody, float InDt, float InSeconds) {
        float sim = 0.0f;
        while (sim < InSeconds) {
            InScene.Tick(InDt);
            sim += InDt;
        }
        glm::vec3 loc;
        glm::quat rot;
        InBody.GetTransform(loc, rot);
        return loc.y;
    }

} // namespace

TEST_SUITE("Jolt physics scene") {
    TEST_CASE_FIXTURE(FJoltPhysicsFixture, "create and destroy bodies does not leak") {
        auto world = Leon::UWorld::Create("JoltLeak");
        auto* scene = world->GetPhysicsScene();
        REQUIRE(scene);
        const int32_t before = scene->GetRigidBodyCount();
        std::vector<Leon::IPhysicsBody*> bodies;
        bodies.reserve(32);
        for (int i = 0; i < 32; ++i) {
            Leon::FPhysicsBodyCreateInfo info;
            info.Location = {static_cast<float>(i), 2.0f, 0.0f};
            bodies.push_back(scene->CreateRigidBody(info));
            REQUIRE(bodies.back());
        }
        CHECK(scene->GetRigidBodyCount() == before + 32);
        for (Leon::IPhysicsBody* body : bodies)
            scene->DestroyRigidBody(body);
        CHECK(scene->GetRigidBodyCount() == before);
    }

    TEST_CASE_FIXTURE(FJoltPhysicsFixture, "dynamic box rests on static box") {
        auto world = Leon::UWorld::Create("JoltStack");
        auto* scene = world->GetPhysicsScene();
        REQUIRE(scene);
        auto* floor = CreateBox(*scene, {0.0f, 0.0f, 0.0f}, {4.0f, 0.5f, 4.0f}, false);
        auto* falling = CreateBox(*scene, {0.0f, 5.0f, 0.0f}, {0.5f, 0.5f, 0.5f}, true);
        REQUIRE(floor);
        REQUIRE(falling);
        const float y = RestingHeightAfter(*scene, *falling, Leon::kPhysicsFixedDeltaSeconds, 2.0f);
        CHECK(y > 0.8f);
        CHECK(y < 1.4f);
        scene->DestroyRigidBody(falling);
        scene->DestroyRigidBody(floor);
        CHECK(scene->GetRigidBodyCount() == 0);
    }

    TEST_CASE_FIXTURE(FJoltPhysicsFixture, "fixed timestep 30 60 120 Hz agree") {
        auto simulate = [](float InDt) {
            auto world = Leon::UWorld::Create("JoltHz");
            auto* scene = world->GetPhysicsScene();
            REQUIRE(scene);
            CreateBox(*scene, {0.0f, 0.0f, 0.0f}, {4.0f, 0.5f, 4.0f}, false);
            auto* falling = CreateBox(*scene, {0.0f, 5.0f, 0.0f}, {0.5f, 0.5f, 0.5f}, true);
            REQUIRE(falling);
            return RestingHeightAfter(*scene, *falling, InDt, 2.0f);
        };
        const float y30 = simulate(1.0f / 30.0f);
        const float y60 = simulate(1.0f / 60.0f);
        const float y120 = simulate(1.0f / 120.0f);
        CHECK(y30 == doctest::Approx(y60).epsilon(0.2f));
        CHECK(y60 == doctest::Approx(y120).epsilon(0.2f));
    }

    TEST_CASE_FIXTURE(FJoltPhysicsFixture, "box component queries after BeginPlay") {
        auto world = Leon::UWorld::Create("JoltQuery");
        auto* wall = world->SpawnActor<Leon::AActor>("Wall");
        wall->SetActorLocation({0.0f, 1.0f, 8.0f});
        auto box = wall->AddActorComponent<Leon::UBoxComponent>("Box");
        box->SetBoxExtent({0.5f, 0.5f, 0.5f});
        box->SetCollisionObjectType(Leon::ECollisionChannel::WorldStatic);
        world->BeginPlay();

        Leon::FHitResult hit;
        CHECK(
            world->LineTraceSingleByChannel({0, 1, 0}, {0, 1, 20}, Leon::ECollisionChannel::Visibility, nullptr, hit));
        CHECK(hit.bBlockingHit);
        CHECK(hit.Actor == wall);
        CHECK(world->SweepSingleByChannel({0, 1, 0}, {0, 1, 20}, 0.2f, Leon::ECollisionChannel::WorldStatic, nullptr,
                                          hit));
        CHECK(world->OverlapAnyTestByChannel({0, 1, 8}, {0.6f, 0.6f, 0.6f}, Leon::ECollisionChannel::WorldStatic,
                                             nullptr));
    }

    TEST_CASE_FIXTURE(FJoltPhysicsFixture, "implicit ECS mesh collider") {
        auto world = Leon::UWorld::Create("JoltEcs");
        auto* floor = world->SpawnActor<Leon::AActor>("Floor");
        floor->SetActorLocation({0.0f, 0.0f, 5.0f});
        auto& mesh = floor->AddComponent<Leon::FMeshComponent>();
        mesh.MeshType = "Cube";
        mesh.MeshSize = 2.0f;
        mesh.Mobility = Leon::EComponentMobility::Static;
        world->BeginPlay();

        Leon::FHitResult hit;
        CHECK(
            world->LineTraceSingleByChannel({0, 0, 0}, {0, 0, 10}, Leon::ECollisionChannel::Visibility, nullptr, hit));
        CHECK(hit.Actor == floor);
    }

    TEST_CASE_FIXTURE(FJoltPhysicsFixture, "static triangle mesh uses MeshShape queries") {
        auto world = Leon::UWorld::Create("JoltMesh");
        auto mesh = Leon::UStaticMesh::Create("TriFloor");
        Leon::FStaticMeshVertex a{};
        a.Position = {-4.0f, 0.0f, -4.0f};
        a.Normal = {0.0f, 1.0f, 0.0f};
        Leon::FStaticMeshVertex b = a;
        b.Position = {4.0f, 0.0f, -4.0f};
        Leon::FStaticMeshVertex c = a;
        c.Position = {4.0f, 0.0f, 4.0f};
        Leon::FStaticMeshVertex d = a;
        d.Position = {-4.0f, 0.0f, 4.0f};
        mesh->GetVertices() = {a, b, c, d};
        mesh->GetIndices() = {0, 1, 2, 0, 2, 3};
        mesh->CalculateBounds();

        auto* floor = world->SpawnActor<Leon::AActor>("MeshFloor");
        floor->SetActorLocation({0.0f, 0.0f, 0.0f});
        auto& smc = floor->AddComponent<Leon::FStaticMeshComponent>();
        smc.StaticMesh = mesh;
        smc.Mobility = Leon::EComponentMobility::Static;
        world->BeginPlay();
        CHECK(world->GetPhysicsScene()->GetRigidBodyCount() >= 1);

        Leon::FHitResult hit;
        CHECK(world->LineTraceSingleByChannel({0.0f, 2.0f, 0.0f}, {0.0f, -2.0f, 0.0f},
                                              Leon::ECollisionChannel::Visibility, nullptr, hit));
        CHECK(hit.bBlockingHit);
        CHECK(hit.Actor == floor);
        CHECK(hit.Location.y == doctest::Approx(0.0f).epsilon(0.05f));

        Leon::FHitResult sweep;
        CHECK(world->SweepSingleByChannel({0.0f, 2.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, 0.3f,
                                          Leon::ECollisionChannel::WorldStatic, nullptr, sweep));
        CHECK(sweep.bBlockingHit);
    }

    TEST_CASE_FIXTURE(FJoltPhysicsFixture, "CMC walks into a wall") {
        auto world = Leon::UWorld::Create("JoltCmc");
        auto* floor = world->SpawnActor<Leon::AActor>("Floor");
        floor->SetActorLocation({0.0f, -0.25f, 0.0f});
        floor->SetActorScale({40.0f, 0.5f, 40.0f});
        auto floorBox = floor->AddActorComponent<Leon::UBoxComponent>("FloorBox");
        floorBox->SetBoxExtent({0.5f, 0.5f, 0.5f});

        auto* wall = world->SpawnActor<Leon::AActor>("Wall");
        wall->SetActorLocation({0.0f, 1.0f, 4.0f});
        auto wallBox = wall->AddActorComponent<Leon::UBoxComponent>("WallBox");
        wallBox->SetBoxExtent({2.0f, 2.0f, 0.25f});

        auto* ch = world->SpawnActor<Leon::ACharacter>("Walker");
        ch->SetFloorZ(0.0f);
        ch->SetActorLocation({0.0f, 0.9f, 0.0f});
        world->BeginPlay();

        for (int i = 0; i < 90; ++i) {
            ch->AddMovementInput({0.0f, 0.0f, 1.0f}, 1.0f);
            world->Tick(Leon::FTimestep(1.0f / 60.0f));
        }
        CHECK(ch->GetActorLocation().z < 3.6f);
    }

    TEST_CASE_FIXTURE(FJoltPhysicsFixture, "thin wall projectile substep") {
        auto world = Leon::UWorld::Create("JoltThin");
        auto* wall = world->SpawnActor<Leon::AActor>("ThinWall");
        wall->SetActorLocation({0.0f, 1.0f, 2.0f});
        auto box = wall->AddActorComponent<Leon::UBoxComponent>("Box");
        box->SetBoxExtent({2.0f, 2.0f, 0.02f});
        world->BeginPlay();

        auto* bolt = world->SpawnActor<Leon::AActor>("Bolt");
        bolt->SetActorLocation({0.0f, 1.0f, 0.0f});
        auto move = bolt->AddActorComponent<Leon::UProjectileMovementComponent>("Move");
        move->SetUpdatedComponent(bolt);
        move->ProjectileGravityScale = 0.0f;
        move->ProjectileRadius = 0.18f;
        move->SetVelocity({0.0f, 0.0f, 80.0f});
        move->Tick(1.0f / 30.0f);
        CHECK(bolt->GetActorLocation().z < 2.4f);
    }

    TEST_CASE_FIXTURE(FJoltPhysicsFixture, "CreateHumanoidFromSkeleton fills constraints") {
        auto skel = Leon::USkeleton::Create("Humanoid");
        Leon::FSkeletonBone hips;
        hips.Name = "hips";
        hips.ParentIndex = -1;
        Leon::FSkeletonBone spine;
        spine.Name = "spine";
        spine.ParentIndex = 0;
        Leon::FSkeletonBone head;
        head.Name = "head";
        head.ParentIndex = 1;
        skel->GetBones() = {hips, spine, head};
        skel->RebuildLookup();

        auto asset = Leon::UPhysicsAsset::CreateHumanoidFromSkeleton(*skel);
        REQUIRE(asset);
        CHECK(asset->GetBodies().size() >= 2);
        CHECK_FALSE(asset->GetConstraints().empty());
        CHECK(asset->GetConstraints().front().Type == Leon::EPhysicsConstraintType::SwingTwist);
    }

    TEST_CASE_FIXTURE(FJoltPhysicsFixture, "overlap normal pushes the query out of a wall") {
        auto world = Leon::UWorld::Create("JoltOverlapN");
        auto* wall = world->SpawnActor<Leon::AActor>("Wall");
        wall->SetActorLocation({0.0f, 1.0f, 4.0f});
        auto box = wall->AddActorComponent<Leon::UBoxComponent>("Box");
        box->SetBoxExtent({2.0f, 2.0f, 0.25f});
        world->BeginPlay();

        std::vector<Leon::FHitResult> hits;
        REQUIRE(world->OverlapMultiByChannel({0.0f, 1.0f, 3.6f}, {0.4f, 0.9f, 0.4f},
                                             Leon::ECollisionChannel::WorldStatic, nullptr, hits) > 0);
        CHECK(hits.front().Normal.z < -0.5f);
        CHECK(hits.front().PenetrationDepth > 0.0f);
    }

    TEST_CASE_FIXTURE(FJoltPhysicsFixture, "dense floor mesh does not hide a wall sweep") {
        auto world = Leon::UWorld::Create("JoltDenseFloor");
        auto mesh = Leon::UStaticMesh::Create("DenseFloor");
        std::vector<Leon::FStaticMeshVertex> verts;
        std::vector<uint32_t> indices;
        const int n = 12;
        verts.resize(static_cast<size_t>((n + 1) * (n + 1)));
        for (int z = 0; z <= n; ++z) {
            for (int x = 0; x <= n; ++x) {
                Leon::FStaticMeshVertex v{};
                v.Position = {static_cast<float>(x) * 0.5f - 3.0f, 0.0f, static_cast<float>(z) * 0.5f - 3.0f};
                v.Normal = {0.0f, 1.0f, 0.0f};
                verts[static_cast<size_t>(z * (n + 1) + x)] = v;
            }
        }
        for (int z = 0; z < n; ++z) {
            for (int x = 0; x < n; ++x) {
                const uint32_t i0 = static_cast<uint32_t>(z * (n + 1) + x);
                const uint32_t i1 = i0 + 1;
                const uint32_t i2 = i0 + static_cast<uint32_t>(n + 1);
                const uint32_t i3 = i2 + 1;
                indices.insert(indices.end(), {i0, i2, i1, i1, i2, i3});
            }
        }
        mesh->GetVertices() = std::move(verts);
        mesh->GetIndices() = std::move(indices);
        mesh->CalculateBounds();

        auto* floor = world->SpawnActor<Leon::AActor>("Floor");
        auto& smc = floor->AddComponent<Leon::FStaticMeshComponent>();
        smc.StaticMesh = mesh;
        smc.Mobility = Leon::EComponentMobility::Static;

        auto* wall = world->SpawnActor<Leon::AActor>("Wall");
        wall->SetActorLocation({0.0f, 1.0f, 4.0f});
        auto wallBox = wall->AddActorComponent<Leon::UBoxComponent>("WallBox");
        wallBox->SetBoxExtent({2.0f, 2.0f, 0.25f});
        world->BeginPlay();

        Leon::FHitResult hit;
        REQUIRE(world->SweepSingleByChannel({0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 8.0f}, 0.4f,
                                            Leon::ECollisionChannel::WorldStatic, nullptr, hit));
        CHECK(hit.Actor == wall);
        CHECK(hit.Normal.z < -0.5f);
    }

    TEST_CASE_FIXTURE(FJoltPhysicsFixture, "sweep and overlap hit locations are world space far from origin") {
        auto world = Leon::UWorld::Create("JoltHitWorldSpace");
        const glm::vec3 wallCenter = {100.0f, 1.0f, 108.0f};
        auto* wall = world->SpawnActor<Leon::AActor>("FarWall");
        wall->SetActorLocation(wallCenter);
        auto wallBox = wall->AddActorComponent<Leon::UBoxComponent>("WallBox");
        wallBox->SetBoxExtent({2.0f, 2.0f, 0.25f});
        wallBox->SetCollisionObjectType(Leon::ECollisionChannel::WorldStatic);
        world->BeginPlay();

        Leon::FHitResult sweep;
        REQUIRE(world->SweepSingleByChannel({100.0f, 1.0f, 100.0f}, {100.0f, 1.0f, 120.0f}, 0.2f,
                                            Leon::ECollisionChannel::WorldStatic, nullptr, sweep));
        CHECK(sweep.bBlockingHit);
        CHECK(sweep.Actor == wall);
        // Regression: contacts used to be left relative to sweep start → impacts at map origin.
        CHECK(sweep.Location.x == doctest::Approx(100.0f).epsilon(0.05f));
        CHECK(sweep.Location.y == doctest::Approx(1.0f).epsilon(0.5f));
        CHECK(sweep.Location.z == doctest::Approx(107.75f).epsilon(0.35f));
        CHECK(glm::length(sweep.Location - wallCenter) < 3.0f);

        Leon::FHitResult overlap;
        std::vector<Leon::FHitResult> overlaps;
        REQUIRE(world->OverlapMultiByChannel({100.0f, 1.0f, 108.0f}, {0.6f, 0.6f, 0.6f},
                                             Leon::ECollisionChannel::WorldStatic, nullptr, overlaps) > 0);
        overlap = overlaps.front();
        CHECK(overlap.Location.x == doctest::Approx(100.0f).epsilon(0.5f));
        CHECK(glm::length(overlap.Location - wallCenter) < 3.0f);
    }

    TEST_CASE_FIXTURE(FJoltPhysicsFixture, "ragdoll writeback keeps component bone scale") {
        auto world = Leon::UWorld::Create("JoltRagdollScale");
        auto skel = Leon::USkeleton::Create("ScaledHumanoid");
        Leon::FSkeletonBone hips;
        hips.Name = "hips";
        hips.ParentIndex = -1;
        hips.RestLocal.Translation = {0.0f, 0.9f, 0.0f};
        hips.RestLocal.Scale = {0.01f, 0.01f, 0.01f};
        Leon::FSkeletonBone spine;
        spine.Name = "spine";
        spine.ParentIndex = 0;
        spine.RestLocal.Translation = {0.0f, 0.2f, 0.0f};
        spine.RestLocal.Scale = {0.01f, 0.01f, 0.01f};
        skel->GetBones() = {hips, spine};
        skel->RebuildLookup();

        auto skm = Leon::USkeletalMesh::Create("ScaledMesh");
        skm->SetSkeleton(skel);
        auto* ch = world->SpawnActor<Leon::ACharacter>("RagdollChar");
        REQUIRE(ch->GetMesh());
        ch->GetMesh()->SetSkeletalMesh(skm);
        ch->GetMesh()->SetPhysicsAsset(Leon::UPhysicsAsset::CreateHumanoidFromSkeleton(*skel));
        world->BeginPlay();

        REQUIRE(ch->GetMesh()->TryEnableRagdoll({0.0f, 1.0f, 0.0f}));
        world->GetPhysicsScene()->Tick(Leon::kPhysicsFixedDeltaSeconds);
        ch->GetMesh()->ApplyRagdollPoseFromBodies();
        REQUIRE(ch->GetMesh()->GetComponentSpaceTransforms().size() >= 1);
        const glm::mat4& hipsM = ch->GetMesh()->GetComponentSpaceTransforms()[0];
        const float sx = glm::length(glm::vec3(hipsM[0]));
        CHECK(sx == doctest::Approx(0.01f).epsilon(0.25f));
        CHECK(sx < 0.5f);
    }
}
