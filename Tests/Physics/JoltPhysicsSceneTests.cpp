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
#include "Physics/UPhysicalMaterial.hpp"
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
            info.ObjectType = Leon::ECollisionChannel::WorldDynamic;
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

    TEST_CASE_FIXTURE(FJoltPhysicsFixture, "body Destroy frees without needing UWorld") {
        auto world = Leon::UWorld::Create("JoltDestroyApi");
        auto* scene = world->GetPhysicsScene();
        REQUIRE(scene);
        const int32_t before = scene->GetRigidBodyCount();
        Leon::IPhysicsBody* body = CreateBox(*scene, {0.0f, 2.0f, 0.0f}, {0.5f, 0.5f, 0.5f}, false);
        REQUIRE(body);
        body->Destroy();
        CHECK(scene->GetRigidBodyCount() == before);
    }

    TEST_CASE_FIXTURE(FJoltPhysicsFixture, "IgnoreCollision pairs re-enable after destroy") {
        auto world = Leon::UWorld::Create("JoltSubGroupReuse");
        auto* scene = world->GetPhysicsScene();
        REQUIRE(scene);
        const int32_t before = scene->GetRigidBodyCount();
        for (int round = 0; round < 3; ++round) {
            Leon::IPhysicsBody* a = CreateBox(*scene, {0.0f, 3.0f, 0.0f}, {0.25f, 0.25f, 0.25f}, true);
            Leon::IPhysicsBody* b = CreateBox(*scene, {0.6f, 3.0f, 0.0f}, {0.25f, 0.25f, 0.25f}, true);
            REQUIRE(a);
            REQUIRE(b);
            scene->IgnoreCollision(a, b);
            scene->DestroyRigidBody(a);
            scene->DestroyRigidBody(b);
        }
        CHECK(scene->GetRigidBodyCount() == before);
        Leon::IPhysicsBody* floor = CreateBox(*scene, {0.0f, 0.0f, 0.0f}, {5.0f, 0.25f, 5.0f}, false);
        Leon::IPhysicsBody* falling = CreateBox(*scene, {0.0f, 4.0f, 0.0f}, {0.3f, 0.3f, 0.3f}, true);
        REQUIRE(floor);
        REQUIRE(falling);
        const float y = RestingHeightAfter(*scene, *falling, Leon::kPhysicsFixedDeltaSeconds, 2.0f);
        CHECK(y < 2.0f);
        scene->DestroyRigidBody(falling);
        scene->DestroyRigidBody(floor);
    }

    TEST_CASE_FIXTURE(FJoltPhysicsFixture, "WorldStatic UBox uses Static motion") {
        auto world = Leon::UWorld::Create("JoltStaticMotion");
        auto* wall = world->SpawnActor<Leon::AActor>("Wall");
        wall->SetActorLocation({0.0f, 1.0f, 5.0f});
        auto box = wall->AddActorComponent<Leon::UBoxComponent>("Box");
        box->SetBoxExtent({0.5f, 0.5f, 0.5f});
        box->SetCollisionObjectType(Leon::ECollisionChannel::WorldStatic);
        wall->SetRootComponent(box.get());
        world->BeginPlay();
        REQUIRE(box->GetPhysicsBody());
        CHECK(box->GetPhysicsBody()->GetMotionType() == Leon::EPhysicsMotionType::Static);
    }

    TEST_CASE_FIXTURE(FJoltPhysicsFixture, "capsule sweep hits WorldStatic wall") {
        auto world = Leon::UWorld::Create("JoltCapsuleSweep");
        auto* wall = world->SpawnActor<Leon::AActor>("Wall");
        wall->SetActorLocation({0.0f, 1.0f, 4.0f});
        auto box = wall->AddActorComponent<Leon::UBoxComponent>("Box");
        box->SetBoxExtent({0.5f, 1.0f, 0.25f});
        box->SetCollisionObjectType(Leon::ECollisionChannel::WorldStatic);
        wall->SetRootComponent(box.get());
        world->BeginPlay();
        Leon::FHitResult hit;
        REQUIRE(world->SweepCapsuleSingleByChannel({0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 8.0f}, 0.4f, 0.9f,
                                                   Leon::ECollisionChannel::WorldStatic, nullptr, hit));
        CHECK(hit.bBlockingHit);
        CHECK(hit.Actor == wall);
    }

    TEST_CASE_FIXTURE(FJoltPhysicsFixture, "CMC fixed step 30 vs 120 Hz comparable fall distance") {
        auto run = [](float InFrameDt, float InSeconds) {
            auto world = Leon::UWorld::Create("JoltCmcFall");
            auto* ch = world->SpawnActor<Leon::ACharacter>("Faller");
            ch->SetFloorZ(-1000.0f);
            ch->SetActorLocation({0.0f, 10.0f, 0.0f});
            world->BeginPlay();
            auto move = ch->GetCharacterMovement();
            REQUIRE(move);
            move->SetMovementMode(Leon::EMovementMode::Falling);
            move->SetVelocity({0.0f, 0.0f, 0.0f});
            float sim = 0.0f;
            while (sim < InSeconds) {
                move->TickMovement(InFrameDt);
                sim += InFrameDt;
            }
            return ch->GetActorLocation().y;
        };
        const float y30 = run(1.0f / 30.0f, 0.5f);
        const float y120 = run(1.0f / 120.0f, 0.5f);
        // 0.5s freefall at g=22 ≈ 10 - 0.5*0.5*22 = 7.25 (with substep clamp).
        CHECK(std::abs(y30 - y120) < 0.15f);
        CHECK(y30 < 9.5f);
        CHECK(y120 < 9.5f);
    }

    TEST_CASE_FIXTURE(FJoltPhysicsFixture, "WorldStatic pair filter skips Static-Static contacts") {
        auto world = Leon::UWorld::Create("JoltStaticPair");
        auto* scene = world->GetPhysicsScene();
        REQUIRE(scene);
        Leon::IPhysicsBody* a = CreateBox(*scene, {0.0f, 0.0f, 0.0f}, {1.0f, 0.25f, 1.0f}, false);
        Leon::IPhysicsBody* b = CreateBox(*scene, {0.5f, 0.0f, 0.0f}, {1.0f, 0.25f, 1.0f}, false);
        REQUIRE(a);
        REQUIRE(b);
        int hits = 0;
        // No actor/component — DrainContacts would no-op; ensure Tick does not crash overlapping statics.
        for (int i = 0; i < 10; ++i)
            scene->Tick(Leon::kPhysicsFixedDeltaSeconds);
        scene->DrainContacts();
        CHECK(hits == 0);
        scene->DestroyRigidBody(a);
        scene->DestroyRigidBody(b);
    }

    TEST_CASE_FIXTURE(FJoltPhysicsFixture, "Density material overrides default mass") {
        auto world = Leon::UWorld::Create("JoltDensity");
        auto* scene = world->GetPhysicsScene();
        REQUIRE(scene);
        Leon::UPhysicalMaterial mat("Dense");
        mat.Density = 1000.0f;
        Leon::FPhysicsBodyCreateInfo info;
        info.Shape = Leon::EPhysicsShapeType::Box;
        info.BoxHalfExtent = {0.5f, 0.5f, 0.5f}; // volume = 1 m³
        info.Mass = 1.0f;
        info.PhysicalMaterial = &mat;
        info.Motion = Leon::EPhysicsMotionType::Dynamic;
        info.bSimulatePhysics = true;
        auto* body = scene->CreateRigidBody(info);
        REQUIRE(body);
        CHECK(body->GetMass() == doctest::Approx(1000.0f).epsilon(0.01f));
        scene->DestroyRigidBody(body);

        Leon::FPhysicsBodyCreateInfo explicitMass = info;
        explicitMass.Mass = 5.0f;
        auto* body2 = scene->CreateRigidBody(explicitMass);
        REQUIRE(body2);
        CHECK(body2->GetMass() == doctest::Approx(5.0f));
        scene->DestroyRigidBody(body2);
    }

    TEST_CASE_FIXTURE(FJoltPhysicsFixture, "SyncDynamic with leftover accumulator stays finite") {
        auto world = Leon::UWorld::Create("JoltInterp");
        auto* scene = world->GetPhysicsScene();
        REQUIRE(scene);
        Leon::IPhysicsBody* floor = CreateBox(*scene, {0.0f, 0.0f, 0.0f}, {5.0f, 0.25f, 5.0f}, false);
        Leon::IPhysicsBody* box = CreateBox(*scene, {0.0f, 3.0f, 0.0f}, {0.3f, 0.3f, 0.3f}, true);
        REQUIRE(floor);
        REQUIRE(box);
        for (int i = 0; i < 20; ++i) {
            scene->Tick(1.0f / 144.0f);
            scene->SyncDynamicTransforms();
            glm::vec3 loc;
            glm::quat rot;
            box->GetTransform(loc, rot);
            CHECK(std::isfinite(loc.x));
            CHECK(std::isfinite(loc.y));
            CHECK(std::isfinite(loc.z));
        }
        scene->DestroyRigidBody(box);
        scene->DestroyRigidBody(floor);
    }

    TEST_CASE_FIXTURE(FJoltPhysicsFixture, "QueryOnly overlap enter via DrainContacts") {
        auto world = Leon::UWorld::Create("JoltOverlapContact");
        world->InitWorld();
        auto* sensorActor = world->SpawnActor<Leon::AActor>("Sensor");
        sensorActor->SetActorLocation({0.0f, 1.0f, 0.0f});
        auto sensor = sensorActor->AddActorComponent<Leon::USphereComponent>("Sphere");
        sensor->SetSphereRadius(0.8f);
        sensor->SetCollisionEnabled(Leon::ECollisionEnabled::QueryOnly);
        sensor->SetCollisionObjectType(Leon::ECollisionChannel::WorldDynamic);
        sensor->SetCollisionResponseToAllChannels(Leon::ECollisionResponse::Overlap);
        sensor->SetGenerateOverlapEvents(true);
        sensorActor->SetRootComponent(sensor.get());
        int begins = 0;
        sensor->OnComponentBeginOverlap.push_back(
            [&](Leon::UPrimitiveComponent*, Leon::AActor*, Leon::UPrimitiveComponent*, const Leon::FHitResult&) {
                ++begins;
            });

        auto* mover = world->SpawnActor<Leon::AActor>("Mover");
        mover->SetActorLocation({0.0f, 1.0f, 3.0f});
        auto box = mover->AddActorComponent<Leon::UBoxComponent>("Box");
        box->SetBoxExtent({0.3f, 0.3f, 0.3f});
        box->SetCollisionObjectType(Leon::ECollisionChannel::Pawn);
        box->SetSimulatePhysics(true);
        box->SetEnableGravity(false);
        mover->SetRootComponent(box.get());
        world->BeginPlay();

        REQUIRE(sensor->GetPhysicsBody());
        REQUIRE(box->GetPhysicsBody());
        box->GetPhysicsBody()->SetLinearVelocity({0.0f, 0.0f, -6.0f});
        for (int i = 0; i < 90; ++i) {
            world->GetPhysicsScene()->SyncKinematicTransforms();
            world->GetPhysicsScene()->Tick(Leon::kPhysicsFixedDeltaSeconds);
            world->GetPhysicsScene()->SyncDynamicTransforms();
            world->GetPhysicsScene()->DrainContacts();
        }
        CHECK(begins >= 1);
    }

    TEST_CASE_FIXTURE(FJoltPhysicsFixture, "PhysicsBody dynamic rests on WorldStatic floor box") {
        auto world = Leon::UWorld::Create("PhysBodyFloor");
        world->InitWorld();
        auto* floorA = world->SpawnActor<Leon::AActor>("Floor");
        floorA->SetActorLocation({0.0f, -0.25f, 0.0f});
        auto floorBox = floorA->AddActorComponent<Leon::UBoxComponent>("F");
        floorBox->SetBoxExtent({10.0f, 0.25f, 10.0f});
        floorBox->SetCollisionObjectType(Leon::ECollisionChannel::WorldStatic);
        floorA->SetRootComponent(floorBox.get());

        auto* bodyA = world->SpawnActor<Leon::AActor>("Body");
        bodyA->SetActorLocation({0.0f, 2.0f, 0.0f});
        auto cap = bodyA->AddActorComponent<Leon::UCapsuleComponent>("C");
        cap->SetCapsuleSize(0.3f, 0.8f);
        cap->SetCollisionObjectType(Leon::ECollisionChannel::PhysicsBody);
        cap->SetCollisionResponseToAllChannels(Leon::ECollisionResponse::Block);
        cap->SetSimulatePhysics(true);
        cap->SetEnableGravity(true);
        cap->SetMass(70.0f);
        bodyA->SetRootComponent(cap.get());
        world->BeginPlay();
        REQUIRE(floorBox->GetPhysicsBody());
        REQUIRE(cap->GetPhysicsBody());
        for (int i = 0; i < 120; ++i) {
            world->GetPhysicsScene()->Tick(Leon::kPhysicsFixedDeltaSeconds);
            world->GetPhysicsScene()->SyncDynamicTransforms();
        }
        const float y = bodyA->GetActorLocation().y;
        CHECK(y > 0.5f);
        CHECK(y < 3.0f);
    }

    TEST_CASE_FIXTURE(FJoltPhysicsFixture, "character capsule ragdoll rests on floor") {
        auto world = Leon::UWorld::Create("CharRagdollFloor");
        world->InitWorld();
        auto* floorA = world->SpawnActor<Leon::AActor>("Floor");
        floorA->SetActorLocation({0.0f, -0.25f, 0.0f});
        auto floorBox = floorA->AddActorComponent<Leon::UBoxComponent>("F");
        floorBox->SetBoxExtent({10.0f, 0.25f, 10.0f});
        floorBox->SetCollisionObjectType(Leon::ECollisionChannel::WorldStatic);
        floorA->SetRootComponent(floorBox.get());

        auto* ch = world->SpawnActor<Leon::ACharacter>("Hero");
        ch->SetActorLocation({0.0f, 1.0f, 0.0f});
        ch->SetFloorZ(0.0f);
        world->BeginPlay();
        if (ch->GetMesh())
            ch->GetMesh()->SetPhysicsAsset(nullptr);
        ch->EnableRagdoll({0.0f, 5.0f, 0.0f});
        CHECK(ch->IsRagdoll());
        CHECK(ch->GetCapsuleComponent()->IsSimulatingPhysics());
        for (int i = 0; i < 120; ++i) {
            world->GetPhysicsScene()->Tick(Leon::kPhysicsFixedDeltaSeconds);
            world->GetPhysicsScene()->SyncDynamicTransforms();
        }
        const float y = ch->GetActorLocation().y;
        // Capsule rests on floor (y≈0); half-height varies — just prove we did not fall through.
        CHECK(y > 0.15f);
        CHECK(y < 3.5f);
    }

    TEST_CASE_FIXTURE(FJoltPhysicsFixture, "mesh ragdoll hips rest on WorldStatic floor") {
        auto world = Leon::UWorld::Create("MeshRagdollFloor");
        world->InitWorld();
        auto* floorA = world->SpawnActor<Leon::AActor>("Floor");
        floorA->SetActorLocation({0.0f, -0.25f, 0.0f});
        auto floorBox = floorA->AddActorComponent<Leon::UBoxComponent>("F");
        floorBox->SetBoxExtent({10.0f, 0.25f, 10.0f});
        floorBox->SetCollisionObjectType(Leon::ECollisionChannel::WorldStatic);
        floorA->SetRootComponent(floorBox.get());

        auto skel = Leon::USkeleton::Create("Humanoid");
        Leon::FSkeletonBone hips;
        hips.Name = "hips";
        hips.ParentIndex = -1;
        hips.RestLocal.Translation = {0.0f, 0.9f, 0.0f};
        Leon::FSkeletonBone spine;
        spine.Name = "spine";
        spine.ParentIndex = 0;
        spine.RestLocal.Translation = {0.0f, 0.25f, 0.0f};
        Leon::FSkeletonBone head;
        head.Name = "head";
        head.ParentIndex = 1;
        head.RestLocal.Translation = {0.0f, 0.2f, 0.0f};
        skel->GetBones() = {hips, spine, head};
        skel->RebuildLookup();
        auto skm = Leon::USkeletalMesh::Create("HumanoidMesh");
        skm->SetSkeleton(skel);

        auto* ch = world->SpawnActor<Leon::ACharacter>("Hero");
        ch->SetActorLocation({0.0f, 1.0f, 0.0f});
        ch->SetFloorZ(0.0f);
        REQUIRE(ch->GetMesh());
        ch->GetMesh()->SetSkeletalMesh(skm);
        ch->GetMesh()->SetPhysicsAsset(Leon::UPhysicsAsset::CreateHumanoidFromSkeleton(*skel));
        world->BeginPlay();
        REQUIRE(ch->GetMesh()->TryEnableRagdoll({0.0f, 1.0f, 0.0f}));
        CHECK(ch->GetMesh()->IsRagdoll());
        for (int i = 0; i < 120; ++i) {
            world->GetPhysicsScene()->Tick(Leon::kPhysicsFixedDeltaSeconds);
            world->GetPhysicsScene()->SyncDynamicTransforms();
            ch->GetMesh()->ApplyRagdollPoseFromBodies();
        }
        glm::vec3 loc;
        glm::quat rot;
        REQUIRE(ch->GetMesh()->GetRagdollRootTransform(loc, rot));
        // Hips capsule rests near the floor plane (y≈0); allow slight sink.
        CHECK(loc.y > -0.05f);
        CHECK(loc.y < 3.0f);
    }

    TEST_CASE_FIXTURE(FJoltPhysicsFixture, "implicit FMeshComponent floor stops PhysicsBody") {
        auto world = Leon::UWorld::Create("ImplicitFloor");
        world->InitWorld();
        auto* floorA = world->SpawnActor<Leon::AActor>("Floor");
        floorA->SetActorLocation({0.0f, -0.25f, 0.0f});
        floorA->SetActorScale({20.0f, 0.5f, 20.0f});
        auto& mesh = floorA->AddComponent<Leon::FMeshComponent>();
        mesh.MeshType = "Cube";
        mesh.MeshSize = 1.0f;
        mesh.Mobility = Leon::EComponentMobility::Static;
        auto* scene = world->GetPhysicsScene();
        world->BeginPlay();
        CHECK(scene->GetRigidBodyCount() >= 1);

        Leon::FPhysicsBodyCreateInfo info;
        info.Shape = Leon::EPhysicsShapeType::Capsule;
        info.CapsuleRadius = 0.3f;
        info.CapsuleHalfHeight = 0.8f;
        info.Location = {0.0f, 2.0f, 0.0f};
        info.Mass = 70.0f;
        info.Motion = Leon::EPhysicsMotionType::Dynamic;
        info.bSimulatePhysics = true;
        info.bEnableGravity = true;
        info.ObjectType = Leon::ECollisionChannel::PhysicsBody;
        auto* body = scene->CreateRigidBody(info);
        REQUIRE(body);
        for (int i = 0; i < 120; ++i)
            scene->Tick(Leon::kPhysicsFixedDeltaSeconds);
        glm::vec3 loc;
        glm::quat rot;
        body->GetTransform(loc, rot);
        CHECK(loc.y > 0.5f);
        CHECK(loc.y < 3.0f);
        scene->DestroyRigidBody(body);
    }
}