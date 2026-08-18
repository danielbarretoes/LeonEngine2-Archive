#include "FJoltPhysicsDriver.hpp"
#include "Physics/FSimplePhysicsScene.hpp"
#include "Physics/IPhysicsScene.hpp"
#include "Engine/UWorld.hpp"
#include "Engine/ENetTypes.hpp"

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>

#include <algorithm>
#include <cstdarg>
#include <iostream>
#include <thread>

namespace Leon {

    using namespace JPH;

    namespace Layers {
        static constexpr ObjectLayer NON_MOVING = 0;
        static constexpr ObjectLayer MOVING = 1;
        static constexpr ObjectLayer NUM_LAYERS = 2;
    } // namespace Layers

    namespace BroadPhaseLayers {
        static constexpr BroadPhaseLayer NON_MOVING(0);
        static constexpr BroadPhaseLayer MOVING(1);
        static constexpr uint NUM_LAYERS(2);
    } // namespace BroadPhaseLayers

    class FJoltBPLayerInterface final : public BroadPhaseLayerInterface {
    public:
        FJoltBPLayerInterface() {
            ObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
            ObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
        }
        uint GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::NUM_LAYERS; }
        BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer inLayer) const override { return ObjectToBroadPhase[inLayer]; }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
        const char* GetBroadPhaseLayerName(BroadPhaseLayer inLayer) const override {
            return (inLayer == BroadPhaseLayers::NON_MOVING) ? "NON_MOVING" : "MOVING";
        }
#endif

    private:
        BroadPhaseLayer ObjectToBroadPhase[Layers::NUM_LAYERS];
    };

    class FJoltObjectVsBroadPhaseLayerFilter : public ObjectVsBroadPhaseLayerFilter {
    public:
        bool ShouldCollide(ObjectLayer inLayer1, BroadPhaseLayer inLayer2) const override {
            switch (inLayer1) {
            case Layers::NON_MOVING:
                return inLayer2 == BroadPhaseLayers::MOVING;
            case Layers::MOVING:
                return true;
            default:
                return false;
            }
        }
    };

    class FJoltObjectLayerPairFilter : public ObjectLayerPairFilter {
    public:
        bool ShouldCollide(ObjectLayer inObject1, ObjectLayer inObject2) const override {
            switch (inObject1) {
            case Layers::NON_MOVING:
                return inObject2 == Layers::MOVING;
            case Layers::MOVING:
                return true;
            default:
                return false;
            }
        }
    };

    static void JoltTrace(const char* inFMT, ...) {
        va_list list;
        va_start(list, inFMT);
        char buffer[1024];
        vsnprintf(buffer, sizeof(buffer), inFMT, list);
        va_end(list);
        std::cout << buffer;
    }

    class FJoltPhysicsScene final : public FSimplePhysicsScene {
    public:
        FJoltPhysicsScene() {
            if (!bTypesRegistered) {
                RegisterDefaultAllocator();
                Trace = JoltTrace;
                Factory::sInstance = new Factory();
                RegisterTypes();
                bTypesRegistered = true;
            }
            Temp = std::make_unique<TempAllocatorImpl>(8 * 1024 * 1024);
            Jobs = std::make_unique<JobSystemThreadPool>(
                cMaxPhysicsJobs, cMaxPhysicsBarriers,
                std::max(1, static_cast<int>(std::thread::hardware_concurrency()) - 1));
            BPLayers = std::make_unique<FJoltBPLayerInterface>();
            ObjVsBP = std::make_unique<FJoltObjectVsBroadPhaseLayerFilter>();
            ObjVsObj = std::make_unique<FJoltObjectLayerPairFilter>();
            System = std::make_unique<PhysicsSystem>();
            System->Init(1024, 0, 1024, 1024, *BPLayers, *ObjVsBP, *ObjVsObj);
            System->SetGravity(Vec3(0, -22.0f, 0));
        }

        void Tick(float InDeltaSeconds) override {
            // Gameplay pose is CharacterMovement / SimplePhysics on authority.
            // Client worlds skip Jolt integration so SimulatedProxy pawns are not dual-simulated.
            const bool bAuthority = !GetWorld() || GetWorld()->GetNetMode() != ENetMode::Client;
            if (bAuthority && System && Temp && Jobs)
                System->Update(InDeltaSeconds, 1, Temp.get(), Jobs.get());
            FSimplePhysicsScene::Tick(InDeltaSeconds);
        }

        IPhysicsBody* CreateRigidBody(const FPhysicsBodyCreateInfo& InInfo) override {
            IPhysicsBody* body = FSimplePhysicsScene::CreateRigidBody(InInfo);
            if (!System)
                return body;

            RefConst<Shape> shape;
            if (InInfo.Shape == EPhysicsShapeType::Sphere)
                shape = new SphereShape(std::max(InInfo.SphereRadius, 0.01f));
            else if (InInfo.Shape == EPhysicsShapeType::Capsule) {
                float cyl = std::max(InInfo.CapsuleHalfHeight - InInfo.CapsuleRadius, 0.01f);
                shape = new CapsuleShape(cyl, std::max(InInfo.CapsuleRadius, 0.01f));
            } else {
                // Jolt BoxShape requires half-extent >= convex radius (default 5 cm). Thin
                // mirrors/puddles (actor scale 0.05 → 2.5 cm half-extent) must shrink the radius.
                const float hx = std::max(InInfo.BoxHalfExtent.x, 0.01f);
                const float hy = std::max(InInfo.BoxHalfExtent.y, 0.01f);
                const float hz = std::max(InInfo.BoxHalfExtent.z, 0.01f);
                const float convex = std::min(cDefaultConvexRadius, std::min({hx, hy, hz}));
                shape = new BoxShape(Vec3(hx, hy, hz), convex);
            }

            EMotionType motion = EMotionType::Static;
            ObjectLayer layer = Layers::NON_MOVING;
            if (InInfo.Motion == EPhysicsMotionType::Dynamic) {
                motion = EMotionType::Dynamic;
                layer = Layers::MOVING;
            } else if (InInfo.Motion == EPhysicsMotionType::Kinematic) {
                motion = EMotionType::Kinematic;
                layer = Layers::MOVING;
            }

            BodyCreationSettings settings(shape, RVec3(InInfo.Location.x, InInfo.Location.y, InInfo.Location.z),
                                          Quat::sIdentity(), motion, layer);
            settings.mFriction = InInfo.Friction;
            settings.mRestitution = InInfo.Restitution;
            settings.mLinearDamping = InInfo.LinearDamping;
            settings.mAngularDamping = InInfo.AngularDamping;
            settings.mGravityFactor = InInfo.bEnableGravity ? 1.0f : 0.0f;
            BodyInterface& bi = System->GetBodyInterface();
            bi.CreateAndAddBody(settings, InInfo.bSimulatePhysics ? EActivation::Activate : EActivation::DontActivate);
            return body;
        }

    private:
        static inline bool bTypesRegistered = false;
        std::unique_ptr<TempAllocatorImpl> Temp;
        std::unique_ptr<JobSystemThreadPool> Jobs;
        std::unique_ptr<FJoltBPLayerInterface> BPLayers;
        std::unique_ptr<FJoltObjectVsBroadPhaseLayerFilter> ObjVsBP;
        std::unique_ptr<FJoltObjectLayerPairFilter> ObjVsObj;
        std::unique_ptr<PhysicsSystem> System;
    };

    void FJoltPhysicsDriver::Register() {
        FPhysicsModule::Register([]() -> TRef<IPhysicsScene> { return CreateRef<FJoltPhysicsScene>(); });
    }

    const char* FJoltPhysicsDriver::GetJoltVersion() {
        return "5.3.0";
    }

} // namespace Leon
