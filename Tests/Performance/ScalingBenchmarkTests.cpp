#include <doctest/doctest.h>

#include "AI/UBehaviorTree.hpp"
#include "Core/FFrameProfiler.hpp"
#include "Core/FPerformanceTimer.hpp"
#include "Core/FTimestep.hpp"
#include "Engine/UWorld.hpp"
#include "Gameplay/AActor.hpp"
#include "Gameplay/ACharacter.hpp"
#include "Gameplay/AAIController.hpp"
#include "Gameplay/UParticleComponent.hpp"
#include "Lightmass/FLightBaker.hpp"
#include "Lightmass/FLightmapBuilder.hpp"
#include "Physics/IPhysicsScene.hpp"
#include "FJoltPhysicsDriver.hpp"

#include <cmath>
#include <sstream>
#include <string>
#include <vector>

namespace {

    float TimeWorldTicks(Leon::UWorld& InWorld, int InFrames, float InDt = 1.0f / 60.0f) {
        Leon::FPerformanceTimer timer;
        timer.Start();
        for (int i = 0; i < InFrames; ++i)
            InWorld.Tick(Leon::FTimestep(InDt));
        return timer.StopMs();
    }

    void Report(const char* InName, int InCount, float InMs, int InFrames) {
        const float perFrame = InMs / static_cast<float>(InFrames);
        std::ostringstream os;
        os << InName << " count=" << InCount << " total_ms=" << InMs << " per_frame_ms=" << perFrame;
        MESSAGE(os.str());
    }

} // namespace

TEST_SUITE("Performance scaling benchmarks") {
    TEST_CASE("empty world tick cost") {
        auto world = Leon::UWorld::Create("PerfEmpty");
        world->BeginPlay();
        const int frames = 120;
        const float ms = TimeWorldTicks(*world, frames);
        Report("EmptyWorld", 0, ms, frames);
        CHECK(ms / frames < 2.0f);
    }

    TEST_CASE("static actor tick scaling") {
        constexpr int frames = 60;
        std::vector<int> counts = {1, 100, 1000};
        float prevPer = 0.0f;
        for (int count : counts) {
            auto world = Leon::UWorld::Create("PerfActors");
            world->BeginPlay();
            for (int i = 0; i < count; ++i)
                world->SpawnActor<Leon::AActor>("Static" + std::to_string(i));
            const float ms = TimeWorldTicks(*world, frames);
            const float per = ms / static_cast<float>(frames);
            Report("Actors", count, ms, frames);
            if (prevPer > 0.0f)
                CHECK(per < prevPer * 30.0f);
            prevPer = per;
        }
    }

    TEST_CASE("physics body scaling (Jolt vs engine tick)") {
        Leon::FJoltPhysicsDriver::Register();
        constexpr int frames = 60;
        std::vector<int> counts = {10, 100, 500};
        for (int count : counts) {
            auto world = Leon::UWorld::Create("PerfPhys");
            world->BeginPlay();
            auto* scene = world->GetPhysicsScene();
            REQUIRE(scene);
            std::vector<Leon::IPhysicsBody*> bodies;
            bodies.reserve(static_cast<size_t>(count));
            for (int i = 0; i < count; ++i) {
                Leon::FPhysicsBodyCreateInfo info;
                info.Shape = Leon::EPhysicsShapeType::Sphere;
                info.Motion = Leon::EPhysicsMotionType::Dynamic;
                info.bSimulatePhysics = true;
                info.SphereRadius = 0.25f;
                info.Location = {static_cast<float>(i % 20) * 0.6f, 2.0f + static_cast<float>(i / 20) * 0.6f, 0.0f};
                bodies.push_back(scene->CreateRigidBody(info));
                REQUIRE(bodies.back());
            }
            Leon::FPerformanceTimer timer;
            timer.Start();
            for (int f = 0; f < frames; ++f)
                scene->Tick(1.0f / 60.0f);
            const float physMs = timer.StopMs();
            const float worldMs = TimeWorldTicks(*world, frames);
            Report("PhysicsBodies", count, physMs, frames);
            Report("PhysicsWorldTick", count, worldMs, frames);
            CHECK(physMs / frames < 8.0f);
        }
    }

    TEST_CASE("character tick scaling") {
        constexpr int frames = 60;
        std::vector<int> counts = {1, 4, 8, 16};
        for (int count : counts) {
            auto world = Leon::UWorld::Create("PerfChars");
            world->BeginPlay();
            for (int i = 0; i < count; ++i) {
                auto* ch = world->SpawnActor<Leon::ACharacter>("Char" + std::to_string(i));
                ch->SetActorLocation({static_cast<float>(i) * 2.0f, 1.7f, 0.0f});
            }
            const float ms = TimeWorldTicks(*world, frames);
            Report("Characters", count, ms, frames);
            CHECK(ms / frames < 16.0f);
        }
    }

    TEST_CASE("AI controller tick scaling") {
        constexpr int frames = 60;
        std::vector<int> counts = {2, 4, 8, 16};
        for (int count : counts) {
            auto world = Leon::UWorld::Create("PerfAI");
            world->BeginPlay();
            auto tree = Leon::MakeRef<Leon::UBehaviorTree>("BT");
            for (int i = 0; i < count; ++i) {
                auto* ch = world->SpawnActor<Leon::ACharacter>("AIChar" + std::to_string(i));
                auto* ai = world->SpawnActor<Leon::AAIController>("AI" + std::to_string(i));
                world->AddAIController(ai);
                ai->Possess(ch);
                ai->RunBehaviorTree(tree);
            }
            const float ms = TimeWorldTicks(*world, frames);
            Report("AIControllers", count, ms, frames);
            CHECK(ms / frames < 16.0f);
        }
    }

    TEST_CASE("particle component update scaling") {
        constexpr int frames = 60;
        std::vector<int> counts = {10, 100, 1000};
        for (int count : counts) {
            auto world = Leon::UWorld::Create("PerfFx");
            world->BeginPlay();
            auto* actor = world->SpawnActor<Leon::AActor>("Fx");
            auto fx = actor->AddActorComponent<Leon::UParticleComponent>("Particles");
            Leon::FParticleEmitterSettings settings;
            settings.BurstCount = count;
            settings.Lifetime = 10.0f;
            settings.bOneShot = false;
            fx->SetEmitterSettings(settings);
            fx->Activate(true);
            const float ms = TimeWorldTicks(*world, frames);
            Report("Particles", count, ms, frames);
            CHECK(ms / frames < 8.0f);
        }
    }

    TEST_CASE("light baker small scene timing") {
        Leon::FLightBakerScene scene;
        scene.AtlasWidth = 16;
        scene.AtlasHeight = 16;
        Leon::FLightmapChart chart;
        chart.Resolution = 16;
        chart.PackedWidth = 16;
        chart.PackedHeight = 16;
        chart.Scale = {1, 1};
        chart.Bias = {0, 0};
        scene.Charts.push_back(chart);
        Leon::FBakeVertex v0, v1, v2;
        v0.Position = {0, 0, 0};
        v1.Position = {1, 0, 0};
        v2.Position = {0, 0, 1};
        v0.Normal = v1.Normal = v2.Normal = {0, 1, 0};
        v0.LightmapUV = {0, 0};
        v1.LightmapUV = {1, 0};
        v2.LightmapUV = {0, 1};
        v0.Albedo = v1.Albedo = v2.Albedo = {1, 1, 1};
        scene.Vertices = {v0, v1, v2};
        scene.Triangles.push_back({0, 1, 2, 0, false});
        scene.DirectionalLights.push_back({Leon::FDirectionalLight{{0.3f, -1.0f, 0.2f}, {1, 1, 1}, 4.0f}, true});

        Leon::FLightBakerSettings settings;
        settings.SamplesPerTexel = 4;
        settings.NumIndirectBounces = 1;
        settings.bAmbientOcclusion = false;
        settings.Seed = 1;

        std::vector<float> baked;
        Leon::FPerformanceTimer timer;
        timer.Start();
        Leon::FLightBaker::Bake(scene, settings, baked);
        const float ms = timer.StopMs();
        std::ostringstream os;
        os << "LightBaker small scene ms=" << ms << " texels=" << (scene.AtlasWidth * scene.AtlasHeight);
        MESSAGE(os.str());
        CHECK(baked.size() == static_cast<size_t>(scene.AtlasWidth * scene.AtlasHeight * 4));
        CHECK(ms < 5000.0f);
    }
}
