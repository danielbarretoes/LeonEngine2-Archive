#include "FJoltPhysicsDriver.hpp"
#include "Physics/IPhysicsScene.hpp"
#include "Physics/FCollisionQuery.hpp"
#include "Engine/UWorld.hpp"
#include "Engine/ENetTypes.hpp"
#include "Engine/ECollisionChannel.hpp"
#include "Gameplay/AActor.hpp"
#include "Gameplay/UActorComponent.hpp"
#include "Gameplay/UPrimitiveComponent.hpp"
#include "Assets/UStaticMesh.hpp"
#include "Core/FLog.hpp"

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/GroupFilterTable.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyLockMulti.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Constraints/FixedConstraint.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/SwingTwistConstraint.h>
#include <Jolt/Physics/Constraints/Constraint.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <glm/gtc/quaternion.hpp>

namespace Leon {

    using namespace JPH;

    namespace {
        Quat ToJolt(const glm::quat& InQ) {
            return Quat(InQ.x, InQ.y, InQ.z, InQ.w);
        }
        glm::quat FromJolt(const Quat& InQ) {
            return glm::quat(InQ.GetW(), InQ.GetX(), InQ.GetY(), InQ.GetZ());
        }
        Vec3 ToJolt(const glm::vec3& InV) {
            return Vec3(InV.x, InV.y, InV.z);
        }
        glm::vec3 FromJolt(Vec3Arg InV) {
            return glm::vec3(InV.GetX(), InV.GetY(), InV.GetZ());
        }
        RVec3 ToJoltR(const glm::vec3& InV) {
            return RVec3(InV.x, InV.y, InV.z);
        }
        glm::vec3 FromJoltR(RVec3Arg InV) {
            return glm::vec3(InV.GetX(), InV.GetY(), InV.GetZ());
        }

        // Jolt: contact normal is -normalize(mPenetrationAxis). Using the axis as-is pushes
        // the query shape into the collider (tunnels + camera/character shake).
        glm::vec3 ContactNormalFromPenetrationAxis(Vec3Arg InAxis) {
            glm::vec3 n = FromJolt(InAxis);
            const float len2 = glm::dot(n, n);
            if (len2 < 1.0e-12f)
                return glm::vec3(0.0f, 1.0f, 0.0f);
            return n * (-1.0f / std::sqrt(len2));
        }

        BodyID HitBodyId(const RayCastResult& InHit) {
            return InHit.mBodyID;
        }
        BodyID HitBodyId(const ShapeCastResult& InHit) {
            return InHit.mBodyID2;
        }
        BodyID HitBodyId(const CollideShapeResult& InHit) {
            return InHit.mBodyID2;
        }

        ObjectLayer LayerFromChannel(ECollisionChannel InChannel) {
            return static_cast<ObjectLayer>(static_cast<uint8_t>(InChannel));
        }

        ECollisionChannel ChannelFromLayer(ObjectLayer InLayer) {
            return static_cast<ECollisionChannel>(static_cast<uint8_t>(InLayer));
        }

        // Humanoid assets often use Axis=(0,1,0). Jolt SwingTwist defaults PlaneAxis to Y as well;
        // Y×Y = 0 yields a degenerate constraint frame and NaNs on the first ragdoll step.
        void MakeOrthonormalTwistPlane(Vec3Arg InTwist, Vec3& OutTwist, Vec3& OutPlane) {
            OutTwist = InTwist.LengthSq() > 1.0e-8f ? InTwist.Normalized() : Vec3::sAxisY();
            Vec3 hint = std::abs(OutTwist.Dot(Vec3::sAxisY())) < 0.9f ? Vec3::sAxisY() : Vec3::sAxisX();
            OutPlane = (hint - OutTwist * hint.Dot(OutTwist)).Normalized();
        }

        constexpr uint32_t kGroupFilterSubGroups = 2048;
    } // namespace

    class FJoltBPLayerInterface final : public BroadPhaseLayerInterface {
    public:
        uint GetNumBroadPhaseLayers() const override { return 2; }
        BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer inLayer) const override {
            const auto channel = ChannelFromLayer(inLayer);
            if (channel == ECollisionChannel::WorldStatic)
                return BroadPhaseLayer(0);
            return BroadPhaseLayer(1);
        }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
        const char* GetBroadPhaseLayerName(BroadPhaseLayer inLayer) const override {
            return inLayer == BroadPhaseLayer(0) ? "NON_MOVING" : "MOVING";
        }
#endif
    };

    class FJoltObjectVsBroadPhaseLayerFilter : public ObjectVsBroadPhaseLayerFilter {
    public:
        bool ShouldCollide(ObjectLayer, BroadPhaseLayer) const override { return true; }
    };

    class FJoltObjectLayerPairFilter : public ObjectLayerPairFilter {
    public:
        bool ShouldCollide(ObjectLayer, ObjectLayer) const override { return true; }
    };

    static void JoltTrace(const char* inFMT, ...) {
        va_list list;
        va_start(list, inFMT);
        char buffer[1024];
        vsnprintf(buffer, sizeof(buffer), inFMT, list);
        va_end(list);
        LE_CORE_WARN("Jolt: {0}", buffer);
    }

    class FJoltPhysicsScene;

    class FJoltPhysicsBody final : public IPhysicsBody {
    public:
        FJoltPhysicsScene* Scene = nullptr;
        BodyID Id;
        FPhysicsBodyCreateInfo Info;
        uint32_t SubGroupId = 0;
        bool bAdded = false;

        void SetTransform(const glm::vec3& InLocation, const glm::quat& InRotation) override;
        void GetTransform(glm::vec3& OutLocation, glm::quat& OutRotation) const override;
        void AddForce(const glm::vec3& InForce) override;
        void AddImpulse(const glm::vec3& InImpulse) override;
        void AddTorque(const glm::vec3& InTorque) override;
        void AddAngularImpulse(const glm::vec3& InImpulse) override;
        void SetLinearVelocity(const glm::vec3& InVelocity) override;
        void SetAngularVelocity(const glm::vec3& InVelocity) override;
        glm::vec3 GetLinearVelocity() const override;
        glm::vec3 GetAngularVelocity() const override;
        bool IsSimulating() const override;
        void SetSimulatePhysics(bool bSimulate) override;
        void SetMass(float InMass) override;
        float GetMass() const override;
        void SetEnableGravity(bool bEnable) override;
        void SetLinearDamping(float InDamping) override;
        void SetAngularDamping(float InDamping) override;
        void SetFriction(float InFriction) override;
        void SetRestitution(float InRestitution) override;
        void SetCollisionEnabled(ECollisionEnabled InEnabled) override;
        void SetCollisionResponses(const FCollisionResponseContainer& InResponses) override;
        AActor* GetActor() const override { return Info.Actor; }
        UActorComponent* GetComponent() const override { return Info.Component; }
        ECollisionChannel GetObjectType() const override { return Info.ObjectType; }
        ECollisionResponse GetResponseToChannel(ECollisionChannel InChannel) const override {
            return Info.Responses.Get(InChannel);
        }
        ECollisionEnabled GetCollisionEnabled() const override { return Info.CollisionEnabled; }

        BodyInterface& BI() const;
        bool IsValid() const;
    };

    class FJoltPhysicsConstraint final : public IPhysicsConstraint {
    public:
        Constraint* Handle = nullptr;
    };

    class FJoltQueryBodyFilter final : public BodyFilter {
    public:
        AActor* IgnoreActor = nullptr;
        UActorComponent* IgnoreComponent = nullptr;
        ECollisionChannel QueryChannel = ECollisionChannel::Visibility;
        bool ShouldCollide(const BodyID&) const override { return true; }
        bool ShouldCollideLocked(const Body& InBody) const override;
    };

    class FJoltQueryObjectLayerFilter final : public ObjectLayerFilter {
    public:
        ECollisionChannel QueryChannel = ECollisionChannel::Visibility;
        bool ShouldCollide(ObjectLayer InLayer) const override {
            return TraceChannelAccepts(QueryChannel, ChannelFromLayer(InLayer));
        }
    };

    class FJoltQueryBroadPhaseFilter final : public BroadPhaseLayerFilter {
    public:
        ECollisionChannel QueryChannel = ECollisionChannel::Visibility;
        bool ShouldCollide(BroadPhaseLayer InLayer) const override {
            if (QueryChannel == ECollisionChannel::WorldStatic)
                return InLayer == BroadPhaseLayer(0);
            return true;
        }
    };

    // One hit per body (MeshShape would otherwise flood the collector with floor tris
    // and hide walls). Keeps the best early-out fraction for each BodyID.
    template <class CollectorType> class FPerBodyHitCollector : public CollectorType {
    public:
        using ResultType = typename CollectorType::ResultType;
        static constexpr uint32_t kMaxBodies = 32;

        void Reset() override {
            CollectorType::Reset();
            Hits.clear();
        }

        void AddHit(const ResultType& InResult) override {
            const BodyID id = HitBodyId(InResult);
            for (auto& hit : Hits) {
                if (HitBodyId(hit) != id)
                    continue;
                if (InResult.GetEarlyOutFraction() < hit.GetEarlyOutFraction())
                    hit = InResult;
                return;
            }
            if (Hits.size() < kMaxBodies) {
                Hits.push_back(InResult);
                return;
            }
            auto worst = std::max_element(Hits.begin(), Hits.end(), [](const ResultType& a, const ResultType& b) {
                return a.GetEarlyOutFraction() < b.GetEarlyOutFraction();
            });
            if (InResult.GetEarlyOutFraction() >= worst->GetEarlyOutFraction())
                return;
            *worst = InResult;
        }

        void SortHits() {
            std::sort(Hits.begin(), Hits.end(), [](const ResultType& a, const ResultType& b) {
                return a.GetEarlyOutFraction() < b.GetEarlyOutFraction();
            });
        }

        std::vector<ResultType> Hits;
    };

    class FJoltContactListener final : public ContactListener {
    public:
        FJoltPhysicsScene* Scene = nullptr;
        ValidateResult OnContactValidate(const Body& InBody1, const Body& InBody2, RVec3Arg,
                                         const CollideShapeResult&) override {
            auto* a = reinterpret_cast<FJoltPhysicsBody*>(static_cast<uintptr_t>(InBody1.GetUserData()));
            auto* b = reinterpret_cast<FJoltPhysicsBody*>(static_cast<uintptr_t>(InBody2.GetUserData()));
            if (!a || !b)
                return ValidateResult::AcceptAllContactsForThisBodyPair;
            if (a->GetResponseToChannel(b->GetObjectType()) == ECollisionResponse::Ignore ||
                b->GetResponseToChannel(a->GetObjectType()) == ECollisionResponse::Ignore)
                return ValidateResult::RejectAllContactsForThisBodyPair;
            return ValidateResult::AcceptAllContactsForThisBodyPair;
        }
        void OnContactAdded(const Body& InBody1, const Body& InBody2, const ContactManifold& InManifold,
                            ContactSettings&) override;
        void OnContactPersisted(const Body&, const Body&, const ContactManifold&, ContactSettings&) override {}
        void OnContactRemoved(const SubShapeIDPair&) override {}
    };

    class FJoltPhysicsScene final : public IPhysicsScene {
    public:
        FJoltPhysicsScene();
        ~FJoltPhysicsScene() override;

        void SetWorld(UWorld* InWorld) override { World = InWorld; }
        void Tick(float InDeltaSeconds) override;
        void SyncKinematicTransforms() override;
        void SyncDynamicTransforms() override;
        void DrainContacts() override;
        void CreatePhysicsState(AActor* InActor) override;
        void DestroyPhysicsState(AActor* InActor) override;
        void NotifyBeginPlayFinished() override;
        void IgnoreCollision(IPhysicsBody* InBodyA, IPhysicsBody* InBodyB) override;
        int32_t GetRigidBodyCount() const override;

        IPhysicsBody* CreateRigidBody(const FPhysicsBodyCreateInfo& InInfo) override;
        void DestroyRigidBody(IPhysicsBody* InBody) override;
        IPhysicsConstraint* CreateConstraint(const FPhysicsConstraintCreateInfo& InInfo) override;
        void DestroyConstraint(IPhysicsConstraint* InConstraint) override;

        bool LineTraceSingleByChannel(const glm::vec3& InStart, const glm::vec3& InEnd, ECollisionChannel InChannel,
                                      AActor* InIgnore, FHitResult& OutHit) const override;
        int32_t LineTraceMultiByChannel(const glm::vec3& InStart, const glm::vec3& InEnd, ECollisionChannel InChannel,
                                        AActor* InIgnore, std::vector<FHitResult>& OutHits) const override;
        bool SweepSingleByChannel(const glm::vec3& InStart, const glm::vec3& InEnd, float InRadius,
                                  ECollisionChannel InChannel, AActor* InIgnore, FHitResult& OutHit) const override;
        int32_t SweepMultiByChannel(const glm::vec3& InStart, const glm::vec3& InEnd, float InRadius,
                                    ECollisionChannel InChannel, AActor* InIgnore,
                                    std::vector<FHitResult>& OutHits) const override;
        bool OverlapAnyTestByChannel(const glm::vec3& InPos, const glm::vec3& InHalfExtent, ECollisionChannel InChannel,
                                     AActor* InIgnore) const override;
        int32_t OverlapMultiByChannel(const glm::vec3& InPos, const glm::vec3& InHalfExtent,
                                      ECollisionChannel InChannel, AActor* InIgnore,
                                      std::vector<FHitResult>& OutHits) const override;

        PhysicsSystem* GetSystem() const { return System.get(); }
        void EnqueueContact(const FPhysicsContact& InContact);

        BodyInterface& GetBodyInterface() { return System->GetBodyInterface(); }
        const BodyInterface& GetBodyInterface() const { return System->GetBodyInterface(); }
        const BodyLockInterface& GetLockInterface() const { return System->GetBodyLockInterface(); }

        GroupFilterTable* GetGroupFilter() const { return CollisionFilter.GetPtr(); }
        void ApplyAddedState(FJoltPhysicsBody& InBody, bool bAdd);

    private:
        RefConst<Shape> MakeShape(const FPhysicsBodyCreateInfo& InInfo) const;
        RefConst<Shape> GetOrCreateMeshShape(const UStaticMesh& InMesh) const;
        void FillHitFromBody(const Body& InBody, const glm::vec3& InPoint, const glm::vec3& InNormal, float InDistance,
                             float InLength, ECollisionChannel InChannel, FHitResult& OutHit) const;
        FJoltPhysicsBody* WrapperFromBody(const Body& InBody) const;

        UWorld* World = nullptr;
        std::unique_ptr<TempAllocatorImpl> Temp;
        std::unique_ptr<JobSystem> Jobs;
        std::unique_ptr<FJoltBPLayerInterface> BPLayers;
        std::unique_ptr<FJoltObjectVsBroadPhaseLayerFilter> ObjVsBP;
        std::unique_ptr<FJoltObjectLayerPairFilter> ObjVsObj;
        std::unique_ptr<PhysicsSystem> System;
        std::unique_ptr<FJoltContactListener> Contacts;
        JPH::Ref<GroupFilterTable> CollisionFilter;
        std::vector<std::unique_ptr<FJoltPhysicsBody>> Bodies;
        std::vector<std::unique_ptr<FJoltPhysicsConstraint>> Constraints;
        std::unordered_map<AActor*, std::vector<IPhysicsBody*>> ImplicitBodies;
        mutable std::unordered_map<const UStaticMesh*, RefConst<Shape>> MeshShapeCache;
        std::mutex ContactMutex;
        std::vector<FPhysicsContact> PendingContacts;
        float PhysicsAccumulator = 0.0f;
        uint32_t NextSubGroup = 1;
        static inline bool bTypesRegistered = false;
    };

    BodyInterface& FJoltPhysicsBody::BI() const {
        return Scene->GetBodyInterface();
    }

    bool FJoltPhysicsBody::IsValid() const {
        return Scene && !Id.IsInvalid();
    }

    void FJoltPhysicsBody::SetTransform(const glm::vec3& InLocation, const glm::quat& InRotation) {
        if (!IsValid())
            return;
        BI().SetPositionAndRotation(Id, ToJoltR(InLocation), ToJolt(InRotation),
                                    Info.bSimulatePhysics ? EActivation::Activate : EActivation::DontActivate);
    }

    void FJoltPhysicsBody::GetTransform(glm::vec3& OutLocation, glm::quat& OutRotation) const {
        if (!IsValid())
            return;
        OutLocation = FromJoltR(BI().GetPosition(Id));
        OutRotation = FromJolt(BI().GetRotation(Id));
    }

    void FJoltPhysicsBody::AddForce(const glm::vec3& InForce) {
        if (IsValid())
            BI().AddForce(Id, ToJolt(InForce));
    }
    void FJoltPhysicsBody::AddImpulse(const glm::vec3& InImpulse) {
        if (IsValid())
            BI().AddImpulse(Id, ToJolt(InImpulse));
    }
    void FJoltPhysicsBody::AddTorque(const glm::vec3& InTorque) {
        if (IsValid())
            BI().AddTorque(Id, ToJolt(InTorque));
    }
    void FJoltPhysicsBody::AddAngularImpulse(const glm::vec3& InImpulse) {
        if (IsValid())
            BI().AddAngularImpulse(Id, ToJolt(InImpulse));
    }
    void FJoltPhysicsBody::SetLinearVelocity(const glm::vec3& InVelocity) {
        if (IsValid())
            BI().SetLinearVelocity(Id, ToJolt(InVelocity));
    }
    void FJoltPhysicsBody::SetAngularVelocity(const glm::vec3& InVelocity) {
        if (IsValid())
            BI().SetAngularVelocity(Id, ToJolt(InVelocity));
    }
    glm::vec3 FJoltPhysicsBody::GetLinearVelocity() const {
        return IsValid() ? FromJolt(BI().GetLinearVelocity(Id)) : glm::vec3(0.0f);
    }
    glm::vec3 FJoltPhysicsBody::GetAngularVelocity() const {
        return IsValid() ? FromJolt(BI().GetAngularVelocity(Id)) : glm::vec3(0.0f);
    }
    bool FJoltPhysicsBody::IsSimulating() const {
        return Info.bSimulatePhysics;
    }

    void FJoltPhysicsBody::SetSimulatePhysics(bool bSimulate) {
        Info.bSimulatePhysics = bSimulate;
        if (!IsValid())
            return;
        BI().SetMotionType(Id, bSimulate ? EMotionType::Dynamic : EMotionType::Kinematic,
                           bSimulate ? EActivation::Activate : EActivation::DontActivate);
    }

    void FJoltPhysicsBody::SetMass(float InMass) {
        Info.Mass = std::max(InMass, 0.001f);
        if (!IsValid())
            return;
        BodyLockWrite lock(Scene->GetLockInterface(), Id);
        if (!lock.Succeeded())
            return;
        Body& body = lock.GetBody();
        if (!body.GetMotionPropertiesUnchecked())
            return;
        body.GetMotionProperties()->SetInverseMass(1.0f / Info.Mass);
    }

    float FJoltPhysicsBody::GetMass() const {
        return Info.Mass;
    }

    void FJoltPhysicsBody::SetEnableGravity(bool bEnable) {
        Info.bEnableGravity = bEnable;
        if (IsValid())
            BI().SetGravityFactor(Id, bEnable ? 1.0f : 0.0f);
    }

    void FJoltPhysicsBody::SetLinearDamping(float InDamping) {
        Info.LinearDamping = InDamping;
        if (!IsValid())
            return;
        BodyLockWrite lock(Scene->GetLockInterface(), Id);
        if (lock.Succeeded() && lock.GetBody().GetMotionPropertiesUnchecked())
            lock.GetBody().GetMotionProperties()->SetLinearDamping(InDamping);
    }

    void FJoltPhysicsBody::SetAngularDamping(float InDamping) {
        Info.AngularDamping = InDamping;
        if (!IsValid())
            return;
        BodyLockWrite lock(Scene->GetLockInterface(), Id);
        if (lock.Succeeded() && lock.GetBody().GetMotionPropertiesUnchecked())
            lock.GetBody().GetMotionProperties()->SetAngularDamping(InDamping);
    }

    void FJoltPhysicsBody::SetFriction(float InFriction) {
        Info.Friction = InFriction;
        if (IsValid())
            BI().SetFriction(Id, InFriction);
    }

    void FJoltPhysicsBody::SetRestitution(float InRestitution) {
        Info.Restitution = InRestitution;
        if (IsValid())
            BI().SetRestitution(Id, InRestitution);
    }

    void FJoltPhysicsBody::SetCollisionEnabled(ECollisionEnabled InEnabled) {
        Info.CollisionEnabled = InEnabled;
        if (!IsValid())
            return;
        const bool bWantAdded = InEnabled != ECollisionEnabled::NoCollision;
        if (bWantAdded != bAdded)
            Scene->ApplyAddedState(*this, bWantAdded);
        BodyLockWrite lock(Scene->GetLockInterface(), Id);
        if (lock.Succeeded())
            lock.GetBody().SetIsSensor(InEnabled == ECollisionEnabled::QueryOnly);
    }

    void FJoltPhysicsBody::SetCollisionResponses(const FCollisionResponseContainer& InResponses) {
        Info.Responses = InResponses;
    }

    bool FJoltQueryBodyFilter::ShouldCollideLocked(const Body& InBody) const {
        auto* wrapper = reinterpret_cast<FJoltPhysicsBody*>(static_cast<uintptr_t>(InBody.GetUserData()));
        if (!wrapper)
            return true;
        if (IgnoreActor && wrapper->GetActor() == IgnoreActor)
            return false;
        if (IgnoreComponent && wrapper->GetComponent() == IgnoreComponent)
            return false;
        const ECollisionEnabled enabled = wrapper->GetCollisionEnabled();
        if (enabled == ECollisionEnabled::NoCollision || enabled == ECollisionEnabled::PhysicsOnly)
            return false;
        return wrapper->GetResponseToChannel(QueryChannel) == ECollisionResponse::Block;
    }

    void FJoltContactListener::OnContactAdded(const Body& InBody1, const Body& InBody2,
                                              const ContactManifold& InManifold, ContactSettings&) {
        if (!Scene)
            return;
        auto* a = reinterpret_cast<FJoltPhysicsBody*>(static_cast<uintptr_t>(InBody1.GetUserData()));
        auto* b = reinterpret_cast<FJoltPhysicsBody*>(static_cast<uintptr_t>(InBody2.GetUserData()));
        FPhysicsContact contact;
        contact.ActorA = a ? a->GetActor() : nullptr;
        contact.ActorB = b ? b->GetActor() : nullptr;
        contact.ComponentA = a ? a->GetComponent() : nullptr;
        contact.ComponentB = b ? b->GetComponent() : nullptr;
        contact.Location = FromJoltR(InManifold.mBaseOffset);
        contact.Normal = FromJolt(InManifold.mWorldSpaceNormal);
        contact.bEnter = true;
        Scene->EnqueueContact(contact);
    }

    FJoltPhysicsScene::FJoltPhysicsScene() {
        if (!bTypesRegistered) {
            RegisterDefaultAllocator();
            Trace = JoltTrace;
            Factory::sInstance = new Factory();
            RegisterTypes();
            bTypesRegistered = true;
        }
        Temp = std::make_unique<TempAllocatorImpl>(32 * 1024 * 1024);
        Jobs = std::make_unique<JobSystemSingleThreaded>(cMaxPhysicsJobs);
        BPLayers = std::make_unique<FJoltBPLayerInterface>();
        ObjVsBP = std::make_unique<FJoltObjectVsBroadPhaseLayerFilter>();
        ObjVsObj = std::make_unique<FJoltObjectLayerPairFilter>();
        CollisionFilter = new GroupFilterTable(kGroupFilterSubGroups);
        System = std::make_unique<PhysicsSystem>();
        System->Init(8192, 0, 16384, 16384, *BPLayers, *ObjVsBP, *ObjVsObj);
        System->SetGravity(Vec3(0, -22.0f, 0));
        Contacts = std::make_unique<FJoltContactListener>();
        Contacts->Scene = this;
        System->SetContactListener(Contacts.get());
    }

    FJoltPhysicsScene::~FJoltPhysicsScene() {
        if (System)
            System->SetContactListener(nullptr);
        BodyInterface& bi = GetBodyInterface();
        for (auto& c : Constraints) {
            if (c && c->Handle)
                System->RemoveConstraint(c->Handle);
        }
        Constraints.clear();
        for (auto& body : Bodies) {
            if (!body || body->Id.IsInvalid())
                continue;
            if (body->bAdded)
                bi.RemoveBody(body->Id);
            bi.DestroyBody(body->Id);
        }
        Bodies.clear();
        ImplicitBodies.clear();
        MeshShapeCache.clear();
    }

    void FJoltPhysicsScene::ApplyAddedState(FJoltPhysicsBody& InBody, bool bAdd) {
        BodyInterface& bi = GetBodyInterface();
        if (bAdd && !InBody.bAdded) {
            bi.AddBody(InBody.Id, InBody.Info.bSimulatePhysics ? EActivation::Activate : EActivation::DontActivate);
            InBody.bAdded = true;
        } else if (!bAdd && InBody.bAdded) {
            bi.RemoveBody(InBody.Id);
            InBody.bAdded = false;
        }
    }

    RefConst<Shape> FJoltPhysicsScene::GetOrCreateMeshShape(const UStaticMesh& InMesh) const {
        auto it = MeshShapeCache.find(&InMesh);
        if (it != MeshShapeCache.end())
            return it->second;

        const auto& verts = InMesh.GetVertices();
        const auto& indices = InMesh.GetIndices();
        const size_t triCount = indices.size() / 3;
        if (triCount == 0)
            return {};

        VertexList jverts;
        jverts.reserve(verts.size());
        for (const auto& v : verts) {
            if (!std::isfinite(v.Position.x) || !std::isfinite(v.Position.y) || !std::isfinite(v.Position.z))
                return {};
            jverts.push_back(Float3(v.Position.x, v.Position.y, v.Position.z));
        }

        IndexedTriangleList tris;
        tris.reserve(triCount);
        for (size_t t = 0; t < triCount; ++t) {
            const uint32_t i0 = indices[t * 3 + 0];
            const uint32_t i1 = indices[t * 3 + 1];
            const uint32_t i2 = indices[t * 3 + 2];
            if (i0 >= verts.size() || i1 >= verts.size() || i2 >= verts.size())
                continue;
            const glm::vec3 a = verts[i0].Position;
            const glm::vec3 b = verts[i1].Position;
            const glm::vec3 c = verts[i2].Position;
            if (glm::length(glm::cross(b - a, c - a)) < 1e-10f)
                continue;
            tris.push_back(IndexedTriangle(i0, i1, i2));
        }
        if (tris.empty())
            return {};

        MeshShapeSettings mesh(jverts, tris);
        auto result = mesh.Create();
        if (!result.IsValid()) {
            LE_CORE_WARN("Jolt MeshShape failed for '{0}': {1}", InMesh.GetName(), result.GetError().c_str());
            return {};
        }
        RefConst<Shape> shape = result.Get();
        MeshShapeCache[&InMesh] = shape;
        return shape;
    }

    RefConst<Shape> FJoltPhysicsScene::MakeShape(const FPhysicsBodyCreateInfo& InInfo) const {
        if (InInfo.Shape == EPhysicsShapeType::TriangleMesh && InInfo.CollisionMesh) {
            const glm::mat4& world = InInfo.CollisionMeshWorld;
            const bool bBakeWorld = world != glm::mat4(1.0f);
            if (bBakeWorld) {
                const auto& verts = InInfo.CollisionMesh->GetVertices();
                const auto& indices = InInfo.CollisionMesh->GetIndices();
                VertexList jverts;
                IndexedTriangleList tris;
                jverts.reserve(verts.size());
                for (const auto& v : verts) {
                    glm::vec3 p = glm::vec3(world * glm::vec4(v.Position, 1.0f));
                    jverts.push_back(Float3(p.x, p.y, p.z));
                }
                const size_t triCount = indices.size() / 3;
                tris.reserve(triCount);
                for (size_t t = 0; t < triCount; ++t) {
                    const uint32_t i0 = indices[t * 3 + 0];
                    const uint32_t i1 = indices[t * 3 + 1];
                    const uint32_t i2 = indices[t * 3 + 2];
                    if (i0 >= verts.size() || i1 >= verts.size() || i2 >= verts.size())
                        continue;
                    tris.push_back(IndexedTriangle(i0, i1, i2));
                }
                if (tris.empty())
                    return {};
                MeshShapeSettings mesh(jverts, tris);
                auto result = mesh.Create();
                return result.IsValid() ? result.Get() : RefConst<Shape>{};
            }

            RefConst<Shape> local = GetOrCreateMeshShape(*InInfo.CollisionMesh);
            if (!local) {
                const float hx = std::max(InInfo.BoxHalfExtent.x, 0.01f);
                const float hy = std::max(InInfo.BoxHalfExtent.y, 0.01f);
                const float hz = std::max(InInfo.BoxHalfExtent.z, 0.01f);
                const float convex = std::min(cDefaultConvexRadius, std::min({hx, hy, hz}));
                return new BoxShape(Vec3(hx, hy, hz), convex);
            }
            const glm::vec3 s = glm::abs(InInfo.CollisionMeshScale);
            if (std::abs(s.x - 1.0f) > 1e-4f || std::abs(s.y - 1.0f) > 1e-4f || std::abs(s.z - 1.0f) > 1e-4f)
                return new ScaledShape(local, ToJolt(glm::max(s, glm::vec3(0.01f))));
            return local;
        }
        if (InInfo.Shape == EPhysicsShapeType::Sphere)
            return new SphereShape(std::max(InInfo.SphereRadius, 0.01f));
        if (InInfo.Shape == EPhysicsShapeType::Capsule) {
            float cyl = std::max(InInfo.CapsuleHalfHeight - InInfo.CapsuleRadius, 0.01f);
            return new CapsuleShape(cyl, std::max(InInfo.CapsuleRadius, 0.01f));
        }
        const float hx = std::max(InInfo.BoxHalfExtent.x, 0.01f);
        const float hy = std::max(InInfo.BoxHalfExtent.y, 0.01f);
        const float hz = std::max(InInfo.BoxHalfExtent.z, 0.01f);
        const float convex = std::min(cDefaultConvexRadius, std::min({hx, hy, hz}));
        return new BoxShape(Vec3(hx, hy, hz), convex);
    }

    IPhysicsBody* FJoltPhysicsScene::CreateRigidBody(const FPhysicsBodyCreateInfo& InInfo) {
        auto wrapper = std::make_unique<FJoltPhysicsBody>();
        wrapper->Scene = this;
        wrapper->Info = InInfo;
        if (InInfo.PhysicalMaterial) {
            wrapper->Info.Friction = InInfo.PhysicalMaterial->Friction;
            wrapper->Info.Restitution = InInfo.PhysicalMaterial->Restitution;
        }
        wrapper->SubGroupId = NextSubGroup++;
        if (wrapper->SubGroupId >= kGroupFilterSubGroups)
            wrapper->SubGroupId = (wrapper->SubGroupId % (kGroupFilterSubGroups - 1)) + 1;

        RefConst<Shape> shape = MakeShape(wrapper->Info);
        if (!shape)
            return nullptr;
        EMotionType motion = EMotionType::Static;
        if (InInfo.Motion == EPhysicsMotionType::Dynamic || InInfo.bSimulatePhysics)
            motion = EMotionType::Dynamic;
        else if (InInfo.Motion == EPhysicsMotionType::Kinematic)
            motion = EMotionType::Kinematic;

        BodyCreationSettings settings(shape, ToJoltR(InInfo.Location), ToJolt(InInfo.Rotation), motion,
                                      LayerFromChannel(InInfo.ObjectType));
        settings.mFriction = wrapper->Info.Friction;
        settings.mRestitution = wrapper->Info.Restitution;
        settings.mLinearDamping = InInfo.LinearDamping;
        settings.mAngularDamping = InInfo.AngularDamping;
        settings.mGravityFactor = InInfo.bEnableGravity ? 1.0f : 0.0f;
        settings.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
        settings.mMassPropertiesOverride.mMass = std::max(InInfo.Mass, 0.001f);
        settings.mIsSensor = InInfo.CollisionEnabled == ECollisionEnabled::QueryOnly;
        settings.mMotionQuality = InInfo.bUseCCD ? EMotionQuality::LinearCast : EMotionQuality::Discrete;
        settings.mCollisionGroup = CollisionGroup(CollisionFilter.GetPtr(), CollisionGroup::GroupID(0),
                                                  CollisionGroup::SubGroupID(wrapper->SubGroupId));
        settings.mUserData = 0;

        BodyInterface& bi = GetBodyInterface();
        Body* body = bi.CreateBody(settings);
        if (!body)
            return nullptr;
        wrapper->Id = body->GetID();
        body->SetUserData(static_cast<uint64>(reinterpret_cast<uintptr_t>(wrapper.get())));
        const bool bAdd = InInfo.CollisionEnabled != ECollisionEnabled::NoCollision;
        if (bAdd) {
            bi.AddBody(wrapper->Id, InInfo.bSimulatePhysics ? EActivation::Activate : EActivation::DontActivate);
            wrapper->bAdded = true;
        }
        IPhysicsBody* raw = wrapper.get();
        Bodies.push_back(std::move(wrapper));
        return raw;
    }

    void FJoltPhysicsScene::DestroyRigidBody(IPhysicsBody* InBody) {
        auto* wrapper = dynamic_cast<FJoltPhysicsBody*>(InBody);
        if (!wrapper)
            return;
        BodyInterface& bi = GetBodyInterface();
        if (!wrapper->Id.IsInvalid()) {
            if (wrapper->bAdded)
                bi.RemoveBody(wrapper->Id);
            bi.DestroyBody(wrapper->Id);
        }
        Bodies.erase(
            std::remove_if(Bodies.begin(), Bodies.end(),
                           [wrapper](const std::unique_ptr<FJoltPhysicsBody>& b) { return b.get() == wrapper; }),
            Bodies.end());
    }

    IPhysicsConstraint* FJoltPhysicsScene::CreateConstraint(const FPhysicsConstraintCreateInfo& InInfo) {
        auto* a = dynamic_cast<FJoltPhysicsBody*>(InInfo.BodyA);
        auto* b = dynamic_cast<FJoltPhysicsBody*>(InInfo.BodyB);
        if (!a || !b || a == b || a->Id.IsInvalid() || b->Id.IsInvalid())
            return nullptr;

        IgnoreCollision(a, b);

        glm::vec3 pa, pb;
        glm::quat qa, qb;
        a->GetTransform(pa, qa);
        b->GetTransform(pb, qb);
        glm::vec3 axis = InInfo.Axis;
        if (glm::length(axis) < 1e-5f)
            axis = glm::vec3(0.0f, 1.0f, 0.0f);
        else
            axis = glm::normalize(axis);

        Constraint* constraint = nullptr;
        {
            const BodyLockInterface& locks = System->GetBodyLockInterface();
            const BodyID ids[2] = {a->Id, b->Id};
            BodyLockMultiWrite multi(locks, ids, 2);
            Body* bodyA = multi.GetBody(0);
            Body* bodyB = multi.GetBody(1);
            if (!bodyA || !bodyB)
                return nullptr;

            if (InInfo.Type == EPhysicsConstraintType::Fixed) {
                FixedConstraintSettings s;
                s.mAutoDetectPoint = true;
                constraint = s.Create(*bodyA, *bodyB);
            } else if (InInfo.Type == EPhysicsConstraintType::Hinge) {
                HingeConstraintSettings s;
                s.mPoint1 = ToJoltR(pa);
                s.mPoint2 = ToJoltR(pb);
                Vec3 hingeAxis, hingeNormal;
                MakeOrthonormalTwistPlane(ToJolt(axis), hingeAxis, hingeNormal);
                s.mHingeAxis1 = hingeAxis;
                s.mHingeAxis2 = hingeAxis;
                s.mNormalAxis1 = hingeNormal;
                s.mNormalAxis2 = hingeNormal;
                s.mLimitsMin = -InInfo.Swing1LimitRadians;
                s.mLimitsMax = InInfo.Swing1LimitRadians;
                constraint = s.Create(*bodyA, *bodyB);
            } else if (InInfo.Type == EPhysicsConstraintType::SwingTwist) {
                SwingTwistConstraintSettings s;
                s.mPosition1 = ToJoltR(pa);
                s.mPosition2 = ToJoltR(pb);
                Vec3 twistAxis, planeAxis;
                MakeOrthonormalTwistPlane(ToJolt(axis), twistAxis, planeAxis);
                s.mTwistAxis1 = twistAxis;
                s.mTwistAxis2 = twistAxis;
                s.mPlaneAxis1 = planeAxis;
                s.mPlaneAxis2 = planeAxis;
                s.mPlaneHalfConeAngle = InInfo.Swing1LimitRadians;
                s.mNormalHalfConeAngle = InInfo.Swing2LimitRadians;
                s.mTwistMinAngle = -InInfo.TwistLimitRadians;
                s.mTwistMaxAngle = InInfo.TwistLimitRadians;
                constraint = s.Create(*bodyA, *bodyB);
            } else {
                DistanceConstraintSettings s;
                s.mPoint1 = ToJoltR(pa);
                s.mPoint2 = ToJoltR(pb);
                float rest = InInfo.RestLength;
                if (rest <= 0.0f)
                    rest = glm::length(pb - pa);
                s.mMinDistance = rest;
                s.mMaxDistance = rest;
                constraint = s.Create(*bodyA, *bodyB);
            }
        }
        if (!constraint)
            return nullptr;
        System->AddConstraint(constraint);
        auto wrapped = std::make_unique<FJoltPhysicsConstraint>();
        wrapped->Handle = constraint;
        IPhysicsConstraint* raw = wrapped.get();
        Constraints.push_back(std::move(wrapped));
        return raw;
    }

    void FJoltPhysicsScene::DestroyConstraint(IPhysicsConstraint* InConstraint) {
        auto* wrapped = dynamic_cast<FJoltPhysicsConstraint*>(InConstraint);
        if (!wrapped)
            return;
        if (wrapped->Handle)
            System->RemoveConstraint(wrapped->Handle);
        Constraints.erase(
            std::remove_if(Constraints.begin(), Constraints.end(),
                           [wrapped](const std::unique_ptr<FJoltPhysicsConstraint>& c) { return c.get() == wrapped; }),
            Constraints.end());
    }

    void FJoltPhysicsScene::IgnoreCollision(IPhysicsBody* InBodyA, IPhysicsBody* InBodyB) {
        auto* a = dynamic_cast<FJoltPhysicsBody*>(InBodyA);
        auto* b = dynamic_cast<FJoltPhysicsBody*>(InBodyB);
        if (!a || !b || !CollisionFilter || a->SubGroupId == b->SubGroupId)
            return;
        if (a->SubGroupId >= kGroupFilterSubGroups || b->SubGroupId >= kGroupFilterSubGroups)
            return;
        CollisionFilter->DisableCollision(static_cast<CollisionGroup::SubGroupID>(a->SubGroupId),
                                          static_cast<CollisionGroup::SubGroupID>(b->SubGroupId));
    }

    void FJoltPhysicsScene::Tick(float InDeltaSeconds) {
        if (!System || !Temp || !Jobs)
            return;
        const float clamped = std::min(std::max(InDeltaSeconds, 0.0f), kPhysicsMaxFrameDeltaSeconds);
        PhysicsAccumulator += clamped;
        int32_t steps = 0;
        while (PhysicsAccumulator >= kPhysicsFixedDeltaSeconds && steps < kPhysicsMaxSubsteps) {
            System->Update(kPhysicsFixedDeltaSeconds, 1, Temp.get(), Jobs.get());
            PhysicsAccumulator -= kPhysicsFixedDeltaSeconds;
            ++steps;
        }
        if (steps >= kPhysicsMaxSubsteps)
            PhysicsAccumulator = 0.0f;
    }

    void FJoltPhysicsScene::SyncKinematicTransforms() {
        for (auto& body : Bodies) {
            if (!body || body->IsSimulating())
                continue;
            auto* prim = dynamic_cast<UPrimitiveComponent*>(body->Info.Component);
            if (!prim)
                continue;
            if (prim->GetOwner() && prim->GetOwner()->GetLocalRole() == ENetRole::SimulatedProxy)
                continue;
            const glm::vec3 rot = prim->GetComponentRotation();
            body->SetTransform(prim->GetComponentLocation(), glm::quat(glm::radians(rot)));
        }
    }

    void FJoltPhysicsScene::SyncDynamicTransforms() {
        for (auto& body : Bodies) {
            if (!body || !body->IsSimulating() || !body->Info.Actor)
                continue;
            if (body->Info.Actor->GetLocalRole() == ENetRole::SimulatedProxy)
                continue;
            glm::vec3 loc;
            glm::quat rot;
            body->GetTransform(loc, rot);
            glm::vec3 relative(0.0f);
            if (auto* prim = dynamic_cast<UPrimitiveComponent*>(body->Info.Component))
                relative = prim->GetRelativeLocation();
            body->Info.Actor->SetActorLocation(loc - relative);
            body->Info.Actor->SetActorRotation(glm::degrees(glm::eulerAngles(rot)));
        }
    }

    void FJoltPhysicsScene::EnqueueContact(const FPhysicsContact& InContact) {
        std::lock_guard<std::mutex> lock(ContactMutex);
        PendingContacts.push_back(InContact);
    }

    void FJoltPhysicsScene::DrainContacts() {
        std::vector<FPhysicsContact> local;
        {
            std::lock_guard<std::mutex> lock(ContactMutex);
            local.swap(PendingContacts);
        }
        for (const auto& c : local) {
            if ((c.ActorA && c.ActorA->IsPendingKill()) || (c.ActorB && c.ActorB->IsPendingKill()))
                continue;
            auto fire = [](UActorComponent* Comp, AActor* OtherActor, UActorComponent* OtherComp, const glm::vec3& Loc,
                           const glm::vec3& N) {
                auto* prim = dynamic_cast<UPrimitiveComponent*>(Comp);
                if (!prim)
                    return;
                FHitResult hit;
                hit.Actor = OtherActor;
                hit.Component = dynamic_cast<UPrimitiveComponent*>(OtherComp);
                hit.Location = Loc;
                hit.ImpactPoint = Loc;
                hit.Normal = N;
                hit.ImpactNormal = N;
                hit.bBlockingHit = true;
                for (auto& cb : prim->OnComponentHit) {
                    if (cb)
                        cb(prim, OtherActor, hit.Component, hit);
                }
            };
            fire(c.ComponentA, c.ActorB, c.ComponentB, c.Location, c.Normal);
            fire(c.ComponentB, c.ActorA, c.ComponentA, c.Location, -c.Normal);
        }
    }

    void FJoltPhysicsScene::CreatePhysicsState(AActor* InActor) {
        if (!InActor || ImplicitBodies.count(InActor))
            return;
        for (const auto& comp : InActor->GetActorComponents()) {
            auto* prim = dynamic_cast<UPrimitiveComponent*>(comp.get());
            if (prim && prim->GetPhysicsBody())
                return;
        }
        std::vector<FColliderDesc> colliders;
        GatherActorColliders(*InActor, colliders);
        if (colliders.empty())
            return;
        std::vector<IPhysicsBody*> created;
        for (const auto& c : colliders) {
            if (c.Component)
                continue;
            FPhysicsBodyCreateInfo info;
            info.Actor = InActor;
            info.Shape = c.Shape;
            info.Location = c.Center;
            info.BoxHalfExtent = c.BoxHalfExtent;
            info.SphereRadius = c.SphereRadius;
            info.CapsuleRadius = c.CapsuleRadius;
            info.CapsuleHalfHeight = c.CapsuleHalfHeight;
            info.ObjectType = c.ObjectType;
            info.Responses = c.Responses;
            info.CollisionEnabled = c.CollisionEnabled;
            info.Motion = EPhysicsMotionType::Static;
            info.bSimulatePhysics = false;
            info.CollisionMesh = c.TriangleMesh;
            info.CollisionMeshWorld = glm::mat4(1.0f);
            if (c.Shape == EPhysicsShapeType::TriangleMesh && c.TriangleMesh) {
                info.Location = InActor->GetActorLocation();
                info.Rotation = glm::quat(glm::radians(InActor->GetActorRotation()));
                info.CollisionMeshScale = InActor->GetActorScale();
            }
            IPhysicsBody* body = CreateRigidBody(info);
            if (body)
                created.push_back(body);
        }
        if (!created.empty())
            ImplicitBodies[InActor] = std::move(created);
    }

    void FJoltPhysicsScene::DestroyPhysicsState(AActor* InActor) {
        auto it = ImplicitBodies.find(InActor);
        if (it == ImplicitBodies.end())
            return;
        for (IPhysicsBody* body : it->second)
            DestroyRigidBody(body);
        ImplicitBodies.erase(it);
    }

    int32_t FJoltPhysicsScene::GetRigidBodyCount() const {
        return static_cast<int32_t>(Bodies.size());
    }

    void FJoltPhysicsScene::NotifyBeginPlayFinished() {
        if (System)
            System->OptimizeBroadPhase();
    }

    FJoltPhysicsBody* FJoltPhysicsScene::WrapperFromBody(const Body& InBody) const {
        return reinterpret_cast<FJoltPhysicsBody*>(static_cast<uintptr_t>(InBody.GetUserData()));
    }

    void FJoltPhysicsScene::FillHitFromBody(const Body& InBody, const glm::vec3& InPoint, const glm::vec3& InNormal,
                                            float InDistance, float InLength, ECollisionChannel InChannel,
                                            FHitResult& OutHit) const {
        auto* wrapper = WrapperFromBody(InBody);
        OutHit.bBlockingHit = true;
        OutHit.Location = InPoint;
        OutHit.ImpactPoint = InPoint;
        OutHit.Normal = InNormal;
        OutHit.ImpactNormal = InNormal;
        OutHit.Distance = InDistance;
        OutHit.Time = InLength > 1e-8f ? InDistance / InLength : 0.0f;
        OutHit.Channel = InChannel;
        if (wrapper) {
            OutHit.Actor = wrapper->GetActor();
            OutHit.Component = dynamic_cast<UPrimitiveComponent*>(wrapper->GetComponent());
        }
    }

    bool FJoltPhysicsScene::LineTraceSingleByChannel(const glm::vec3& InStart, const glm::vec3& InEnd,
                                                     ECollisionChannel InChannel, AActor* InIgnore,
                                                     FHitResult& OutHit) const {
        OutHit = {};
        std::vector<FHitResult> hits;
        if (LineTraceMultiByChannel(InStart, InEnd, InChannel, InIgnore, hits) <= 0)
            return false;
        OutHit = hits.front();
        return true;
    }

    int32_t FJoltPhysicsScene::LineTraceMultiByChannel(const glm::vec3& InStart, const glm::vec3& InEnd,
                                                       ECollisionChannel InChannel, AActor* InIgnore,
                                                       std::vector<FHitResult>& OutHits) const {
        OutHits.clear();
        const glm::vec3 delta = InEnd - InStart;
        const float len = glm::length(delta);
        if (len < 1e-8f || !System)
            return 0;
        const glm::vec3 dir = delta / len;
        RRayCast ray{ToJoltR(InStart), ToJolt(dir * len)};
        FPerBodyHitCollector<CastRayCollector> collector;
        RayCastSettings settings;
        settings.mBackFaceModeTriangles = EBackFaceMode::CollideWithBackFaces;
        settings.mBackFaceModeConvex = EBackFaceMode::CollideWithBackFaces;
        FJoltQueryBodyFilter bodyFilter;
        bodyFilter.IgnoreActor = InIgnore;
        bodyFilter.QueryChannel = InChannel;
        FJoltQueryObjectLayerFilter layerFilter;
        layerFilter.QueryChannel = InChannel;
        FJoltQueryBroadPhaseFilter bpFilter;
        bpFilter.QueryChannel = InChannel;
        System->GetNarrowPhaseQuery().CastRay(ray, settings, collector, bpFilter, layerFilter, bodyFilter);
        collector.SortHits();
        for (const RayCastResult& result : collector.Hits) {
            BodyLockRead lock(GetLockInterface(), result.mBodyID);
            if (!lock.Succeeded())
                continue;
            const Body& body = lock.GetBody();
            const glm::vec3 point = InStart + dir * (result.mFraction * len);
            Vec3 n = body.GetWorldSpaceSurfaceNormal(result.mSubShapeID2, ToJoltR(point));
            FHitResult hit;
            FillHitFromBody(body, point, FromJolt(n), result.mFraction * len, len, InChannel, hit);
            OutHits.push_back(hit);
        }
        return static_cast<int32_t>(OutHits.size());
    }

    int32_t FJoltPhysicsScene::SweepMultiByChannel(const glm::vec3& InStart, const glm::vec3& InEnd, float InRadius,
                                                   ECollisionChannel InChannel, AActor* InIgnore,
                                                   std::vector<FHitResult>& OutHits) const {
        OutHits.clear();
        if (!System)
            return 0;
        const glm::vec3 delta = InEnd - InStart;
        const float len = glm::length(delta);
        const float radius = std::max(InRadius, 0.01f);
        RefConst<Shape> sphere = new SphereShape(radius);
        const RMat44 start(RMat44::sTranslation(ToJoltR(InStart)));
        const Vec3 direction = len > 1e-8f ? ToJolt(delta) : Vec3(0, 0, 0);
        RShapeCast shapeCast(sphere, Vec3::sReplicate(1.0f), start, direction);
        FPerBodyHitCollector<CastShapeCollector> collector;
        ShapeCastSettings settings;
        settings.mReturnDeepestPoint = true;
        settings.mBackFaceModeTriangles = EBackFaceMode::CollideWithBackFaces;
        settings.mBackFaceModeConvex = EBackFaceMode::CollideWithBackFaces;
        FJoltQueryBodyFilter bodyFilter;
        bodyFilter.IgnoreActor = InIgnore;
        bodyFilter.QueryChannel = InChannel;
        FJoltQueryObjectLayerFilter layerFilter;
        layerFilter.QueryChannel = InChannel;
        FJoltQueryBroadPhaseFilter bpFilter;
        bpFilter.QueryChannel = InChannel;
        System->GetNarrowPhaseQuery().CastShape(shapeCast, settings, ToJoltR(InStart), collector, bpFilter, layerFilter,
                                                bodyFilter);
        collector.SortHits();
        for (const ShapeCastResult& result : collector.Hits) {
            BodyLockRead lock(GetLockInterface(), result.mBodyID2);
            if (!lock.Succeeded())
                continue;
            const Body& body = lock.GetBody();
            // CastShape returns contacts relative to inBaseOffset (InStart) for precision.
            const glm::vec3 point = InStart + FromJoltR(result.mContactPointOn2);
            const float dist = result.mFraction * len;
            glm::vec3 n = ContactNormalFromPenetrationAxis(result.mPenetrationAxis);
            FHitResult hit;
            FillHitFromBody(body, point, n, dist, len > 1e-8f ? len : 1.0f, InChannel, hit);
            hit.ImpactNormal = hit.Normal;
            OutHits.push_back(hit);
        }
        return static_cast<int32_t>(OutHits.size());
    }

    bool FJoltPhysicsScene::SweepSingleByChannel(const glm::vec3& InStart, const glm::vec3& InEnd, float InRadius,
                                                 ECollisionChannel InChannel, AActor* InIgnore,
                                                 FHitResult& OutHit) const {
        OutHit = {};
        std::vector<FHitResult> hits;
        if (SweepMultiByChannel(InStart, InEnd, InRadius, InChannel, InIgnore, hits) <= 0)
            return false;
        OutHit = hits.front();
        return true;
    }

    int32_t FJoltPhysicsScene::OverlapMultiByChannel(const glm::vec3& InPos, const glm::vec3& InHalfExtent,
                                                     ECollisionChannel InChannel, AActor* InIgnore,
                                                     std::vector<FHitResult>& OutHits) const {
        OutHits.clear();
        if (!System)
            return 0;
        const glm::vec3 he = glm::max(InHalfExtent, glm::vec3(0.01f));
        RefConst<Shape> box = new BoxShape(ToJolt(he));
        const RMat44 com(RMat44::sTranslation(ToJoltR(InPos)));
        FPerBodyHitCollector<CollideShapeCollector> collector;
        CollideShapeSettings settings;
        settings.mBackFaceMode = EBackFaceMode::CollideWithBackFaces;
        FJoltQueryBodyFilter bodyFilter;
        bodyFilter.IgnoreActor = InIgnore;
        bodyFilter.QueryChannel = InChannel;
        FJoltQueryObjectLayerFilter layerFilter;
        layerFilter.QueryChannel = InChannel;
        FJoltQueryBroadPhaseFilter bpFilter;
        bpFilter.QueryChannel = InChannel;
        System->GetNarrowPhaseQuery().CollideShape(box, Vec3::sReplicate(1.0f), com, settings, ToJoltR(InPos),
                                                   collector, bpFilter, layerFilter, bodyFilter);
        for (const CollideShapeResult& result : collector.Hits) {
            BodyLockRead lock(GetLockInterface(), result.mBodyID2);
            if (!lock.Succeeded())
                continue;
            const Body& body = lock.GetBody();
            FHitResult hit;
            glm::vec3 n = ContactNormalFromPenetrationAxis(result.mPenetrationAxis);
            // CollideShape returns contacts relative to inBaseOffset (InPos) for precision.
            FillHitFromBody(body, InPos + FromJoltR(result.mContactPointOn2), n, result.mPenetrationDepth, 1.0f,
                            InChannel, hit);
            hit.PenetrationDepth = result.mPenetrationDepth;
            hit.bStartPenetrating = result.mPenetrationDepth > 0.0f;
            if (glm::length(hit.Normal) > 1e-4f)
                hit.Normal = glm::normalize(hit.Normal);
            hit.ImpactNormal = hit.Normal;
            OutHits.push_back(hit);
        }
        return static_cast<int32_t>(OutHits.size());
    }

    bool FJoltPhysicsScene::OverlapAnyTestByChannel(const glm::vec3& InPos, const glm::vec3& InHalfExtent,
                                                    ECollisionChannel InChannel, AActor* InIgnore) const {
        std::vector<FHitResult> hits;
        return OverlapMultiByChannel(InPos, InHalfExtent, InChannel, InIgnore, hits) > 0;
    }

    void FJoltPhysicsDriver::Register() {
        FPhysicsModule::Register([]() -> TRef<IPhysicsScene> { return CreateRef<FJoltPhysicsScene>(); });
    }

    const char* FJoltPhysicsDriver::GetJoltVersion() {
        return "5.3.0";
    }

} // namespace Leon
