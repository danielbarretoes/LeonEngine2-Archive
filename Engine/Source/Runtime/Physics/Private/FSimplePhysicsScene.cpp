#include "Physics/FSimplePhysicsScene.hpp"
#include "Physics/FCollisionQuery.hpp"
#include "Engine/UWorld.hpp"
#include "Engine/ENetTypes.hpp"
#include "Gameplay/AActor.hpp"
#include "Gameplay/APawn.hpp"
#include "Gameplay/APickup.hpp"
#include "Gameplay/UActorComponent.hpp"
#include "Gameplay/UPrimitiveComponent.hpp"

#include <algorithm>
#include <glm/gtc/quaternion.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace Leon {

    void FSimplePhysicsBody::Destroy() {
        if (Scene)
            Scene->DestroyRigidBody(this);
    }

    void FSimplePhysicsBody::SetTransform(const glm::vec3& InLocation, const glm::quat& InRotation) {
        Location = InLocation;
        Rotation = InRotation;
    }

    void FSimplePhysicsBody::GetTransform(glm::vec3& OutLocation, glm::quat& OutRotation) const {
        OutLocation = Location;
        OutRotation = Rotation;
    }

    void FSimplePhysicsBody::AddForce(const glm::vec3& InForce) {
        PendingForce += InForce;
    }
    void FSimplePhysicsBody::AddImpulse(const glm::vec3& InImpulse) {
        PendingImpulse += InImpulse;
    }
    void FSimplePhysicsBody::AddTorque(const glm::vec3& InTorque) {
        PendingTorque += InTorque;
    }
    void FSimplePhysicsBody::AddAngularImpulse(const glm::vec3& InImpulse) {
        PendingAngularImpulse += InImpulse;
    }
    void FSimplePhysicsBody::SetLinearVelocity(const glm::vec3& InVelocity) {
        LinearVelocity = InVelocity;
    }
    void FSimplePhysicsBody::SetAngularVelocity(const glm::vec3& InVelocity) {
        AngularVelocity = InVelocity;
    }
    glm::vec3 FSimplePhysicsBody::GetLinearVelocity() const {
        return LinearVelocity;
    }
    glm::vec3 FSimplePhysicsBody::GetAngularVelocity() const {
        return AngularVelocity;
    }
    bool FSimplePhysicsBody::IsSimulating() const {
        return bSimulating;
    }
    void FSimplePhysicsBody::SetSimulatePhysics(bool bSimulate) {
        bSimulating = bSimulate;
        Info.bSimulatePhysics = bSimulate;
        if (bSimulate)
            Info.Motion = EPhysicsMotionType::Dynamic;
        else if (Info.ObjectType == ECollisionChannel::WorldStatic)
            Info.Motion = EPhysicsMotionType::Static;
        else
            Info.Motion = EPhysicsMotionType::Kinematic;
    }
    void FSimplePhysicsBody::SetMass(float InMass) {
        Info.Mass = std::max(InMass, 0.001f);
    }
    float FSimplePhysicsBody::GetMass() const {
        return Info.Mass;
    }
    void FSimplePhysicsBody::SetEnableGravity(bool bEnable) {
        Info.bEnableGravity = bEnable;
    }
    void FSimplePhysicsBody::SetLinearDamping(float InDamping) {
        Info.LinearDamping = InDamping;
    }
    void FSimplePhysicsBody::SetAngularDamping(float InDamping) {
        Info.AngularDamping = InDamping;
    }
    void FSimplePhysicsBody::SetFriction(float InFriction) {
        Info.Friction = InFriction;
    }
    void FSimplePhysicsBody::SetRestitution(float InRestitution) {
        Info.Restitution = InRestitution;
    }
    void FSimplePhysicsBody::SetCollisionEnabled(ECollisionEnabled InEnabled) {
        Info.CollisionEnabled = InEnabled;
    }
    void FSimplePhysicsBody::SetCollisionResponses(const FCollisionResponseContainer& InResponses) {
        Info.Responses = InResponses;
    }
    void FSimplePhysicsBody::SetObjectType(ECollisionChannel InType) {
        Info.ObjectType = InType;
    }
    AActor* FSimplePhysicsBody::GetActor() const {
        return Info.Actor;
    }
    UActorComponent* FSimplePhysicsBody::GetComponent() const {
        return Info.Component;
    }
    ECollisionChannel FSimplePhysicsBody::GetObjectType() const {
        return Info.ObjectType;
    }
    ECollisionResponse FSimplePhysicsBody::GetResponseToChannel(ECollisionChannel InChannel) const {
        return Info.Responses.Get(InChannel);
    }
    ECollisionEnabled FSimplePhysicsBody::GetCollisionEnabled() const {
        return Info.CollisionEnabled;
    }
    EPhysicsMotionType FSimplePhysicsBody::GetMotionType() const {
        return Info.Motion;
    }

    void FSimplePhysicsScene::RebuildStaticColliderCache() const {
        CachedStaticColliders.clear();
        CachedActorCount = World ? World->GetAllActors().size() : 0;
        if (!World) {
            bStaticCacheValid = true;
            return;
        }
        for (const auto& actorRef : World->GetAllActors()) {
            AActor* actor = actorRef.get();
            if (!actor || actor->IsPendingKill())
                continue;
            if (dynamic_cast<APawn*>(actor) || dynamic_cast<APickup*>(actor))
                continue;
            GatherActorColliders(*actor, CachedStaticColliders);
        }
        bStaticCacheValid = true;
    }

    void FSimplePhysicsScene::CollectColliders(AActor* InIgnore, std::vector<FColliderDesc>& Out) const {
        Out.clear();
        const size_t actorCount = World ? World->GetAllActors().size() : 0;
        if (!bStaticCacheValid || actorCount != CachedActorCount)
            RebuildStaticColliderCache();
        Out.reserve(CachedStaticColliders.size() + 16);
        for (const auto& c : CachedStaticColliders) {
            if (InIgnore && c.Actor == InIgnore)
                continue;
            Out.push_back(c);
        }
        if (World) {
            for (const auto& actorRef : World->GetAllActors()) {
                AActor* actor = actorRef.get();
                if (!actor || actor == InIgnore || actor->IsPendingKill())
                    continue;
                if (!dynamic_cast<APawn*>(actor) && !dynamic_cast<APickup*>(actor))
                    continue;
                GatherActorColliders(*actor, Out);
            }
        }
        for (const auto& body : Bodies) {
            if (!body)
                continue;
            if (InIgnore && body->Info.Actor == InIgnore)
                continue;
            if (body->Info.Actor)
                continue;
            if (body->Info.CollisionEnabled == ECollisionEnabled::NoCollision ||
                body->Info.CollisionEnabled == ECollisionEnabled::PhysicsOnly)
                continue;
            FColliderDesc d;
            d.Actor = body->Info.Actor;
            d.Component = dynamic_cast<UPrimitiveComponent*>(body->Info.Component);
            d.Shape = body->Info.Shape;
            d.Center = body->Location;
            d.BoxHalfExtent = body->Info.BoxHalfExtent;
            d.SphereRadius = body->Info.SphereRadius;
            d.CapsuleRadius = body->Info.CapsuleRadius;
            d.CapsuleHalfHeight = body->Info.CapsuleHalfHeight;
            d.ObjectType = body->Info.ObjectType;
            d.Responses = body->Info.Responses;
            d.CollisionEnabled = body->Info.CollisionEnabled;
            Out.push_back(d);
        }
    }

    void FSimplePhysicsScene::Integrate(float InDeltaSeconds) {
        const glm::vec3 gravity(0.0f, -22.0f, 0.0f);
        for (auto& body : Bodies) {
            if (!body || !body->bSimulating)
                continue;
            if (body->Info.Actor && body->Info.Actor->GetLocalRole() == ENetRole::SimulatedProxy)
                continue;
            if (body->Info.bEnableGravity)
                body->LinearVelocity += gravity * InDeltaSeconds;
            const float mass = std::max(body->Info.Mass, 0.001f);
            body->LinearVelocity += body->PendingForce * InDeltaSeconds / mass;
            body->LinearVelocity += body->PendingImpulse / mass;
            body->PendingForce = glm::vec3(0.0f);
            body->PendingImpulse = glm::vec3(0.0f);
            body->LinearVelocity *= std::max(0.0f, 1.0f - body->Info.LinearDamping * InDeltaSeconds);

            body->AngularVelocity += body->PendingTorque * InDeltaSeconds / mass;
            body->AngularVelocity += body->PendingAngularImpulse / mass;
            body->PendingTorque = glm::vec3(0.0f);
            body->PendingAngularImpulse = glm::vec3(0.0f);

            const float speed = glm::length(body->LinearVelocity);
            constexpr float kMaxSimSpeed = 40.0f;
            if (speed > kMaxSimSpeed)
                body->LinearVelocity *= kMaxSimSpeed / speed;
            body->Location += body->LinearVelocity * InDeltaSeconds;

            const float angSpeed = glm::length(body->AngularVelocity);
            if (angSpeed > 1e-4f) {
                const glm::vec3 axis = body->AngularVelocity / angSpeed;
                body->Rotation = glm::normalize(glm::angleAxis(angSpeed * InDeltaSeconds, axis) * body->Rotation);
                body->AngularVelocity *= std::max(0.0f, 1.0f - body->Info.AngularDamping * InDeltaSeconds);
            }
        }

        for (int iter = 0; iter < 3; ++iter) {
            for (auto& constraint : Constraints) {
                if (!constraint || !constraint->BodyA || !constraint->BodyB)
                    continue;
                glm::vec3 delta = constraint->BodyB->Location - constraint->BodyA->Location;
                float dist = glm::length(delta);
                if (dist < 1e-5f)
                    continue;
                glm::vec3 dir = delta / dist;
                float error = dist - constraint->RestLength;
                glm::vec3 corr = dir * (error * 0.5f);
                if (constraint->BodyA->bSimulating)
                    constraint->BodyA->Location += corr;
                if (constraint->BodyB->bSimulating)
                    constraint->BodyB->Location -= corr;
            }
        }
    }

    void FSimplePhysicsScene::Tick(float InDeltaSeconds) {
        const float clamped = std::min(std::max(InDeltaSeconds, 0.0f), kPhysicsMaxFrameDeltaSeconds);
        PhysicsAccumulator += clamped;
        int32_t steps = 0;
        while (PhysicsAccumulator >= kPhysicsFixedDeltaSeconds && steps < kPhysicsMaxSubsteps) {
            Integrate(kPhysicsFixedDeltaSeconds);
            PhysicsAccumulator -= kPhysicsFixedDeltaSeconds;
            ++steps;
        }
        if (steps >= kPhysicsMaxSubsteps)
            PhysicsAccumulator = 0.0f;
    }

    void FSimplePhysicsScene::SyncKinematicTransforms() {
        for (auto& body : Bodies) {
            if (!body || body->bSimulating)
                continue;
            if (body->Info.Motion == EPhysicsMotionType::Static)
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

    void FSimplePhysicsScene::SyncDynamicTransforms() {
        for (auto& body : Bodies) {
            if (!body || !body->bSimulating)
                continue;
            if (body->Info.Actor && body->Info.Actor->GetLocalRole() == ENetRole::SimulatedProxy)
                continue;
            if (!body->Info.bSyncComponentTransform)
                continue;
            glm::vec3 loc = body->Location;
            const glm::vec3 euler = glm::degrees(glm::eulerAngles(body->Rotation));
            auto* prim = dynamic_cast<UPrimitiveComponent*>(body->Info.Component);
            if (prim) {
                prim->SetWorldLocationAndRotation(loc, euler);
                continue;
            }
            if (!body->Info.Actor)
                continue;
            body->Info.Actor->SetActorLocation(loc);
            body->Info.Actor->SetActorRotation(euler);
        }
    }

    int32_t FSimplePhysicsScene::GetRigidBodyCount() const {
        return static_cast<int32_t>(Bodies.size());
    }

    IPhysicsBody* FSimplePhysicsScene::CreateRigidBody(const FPhysicsBodyCreateInfo& InInfo) {
        auto body = std::make_unique<FSimplePhysicsBody>();
        body->Scene = this;
        body->Info = InInfo;
        if (InInfo.PhysicalMaterial) {
            body->Info.Friction = InInfo.PhysicalMaterial->Friction;
            body->Info.Restitution = InInfo.PhysicalMaterial->Restitution;
            if (InInfo.Mass == 1.0f && InInfo.Shape != EPhysicsShapeType::TriangleMesh) {
                body->Info.Mass =
                    std::max(0.001f, InInfo.PhysicalMaterial->Density * ApproximatePhysicsShapeVolume(body->Info));
            }
        }
        body->Location = InInfo.Location;
        body->Rotation = InInfo.Rotation;
        body->bSimulating = InInfo.bSimulatePhysics && InInfo.Motion == EPhysicsMotionType::Dynamic;
        IPhysicsBody* raw = body.get();
        Bodies.push_back(std::move(body));
        bStaticCacheValid = false;
        return raw;
    }

    void FSimplePhysicsScene::DestroyRigidBody(IPhysicsBody* InBody) {
        Constraints.erase(std::remove_if(Constraints.begin(), Constraints.end(),
                                         [InBody](const std::unique_ptr<FSimplePhysicsConstraint>& c) {
                                             return c && (c->BodyA == InBody || c->BodyB == InBody);
                                         }),
                          Constraints.end());
        Bodies.erase(
            std::remove_if(Bodies.begin(), Bodies.end(),
                           [InBody](const std::unique_ptr<FSimplePhysicsBody>& b) { return b.get() == InBody; }),
            Bodies.end());
        bStaticCacheValid = false;
    }

    IPhysicsConstraint* FSimplePhysicsScene::CreateConstraint(const FPhysicsConstraintCreateInfo& InInfo) {
        auto* a = dynamic_cast<FSimplePhysicsBody*>(InInfo.BodyA);
        auto* b = dynamic_cast<FSimplePhysicsBody*>(InInfo.BodyB);
        if (!a || !b || a == b)
            return nullptr;
        auto constraint = std::make_unique<FSimplePhysicsConstraint>();
        constraint->BodyA = a;
        constraint->BodyB = b;
        float rest = InInfo.RestLength;
        if (rest <= 0.0f)
            rest = glm::length(b->Location - a->Location);
        constraint->RestLength = rest;
        IPhysicsConstraint* raw = constraint.get();
        Constraints.push_back(std::move(constraint));
        return raw;
    }

    void FSimplePhysicsScene::DestroyConstraint(IPhysicsConstraint* InConstraint) {
        Constraints.erase(std::remove_if(Constraints.begin(), Constraints.end(),
                                         [InConstraint](const std::unique_ptr<FSimplePhysicsConstraint>& c) {
                                             return c.get() == InConstraint;
                                         }),
                          Constraints.end());
    }

    bool FSimplePhysicsScene::LineTraceSingleByChannel(const glm::vec3& InStart, const glm::vec3& InEnd,
                                                       ECollisionChannel InChannel, AActor* InIgnore,
                                                       FHitResult& OutHit) const {
        OutHit = {};
        std::vector<FHitResult> hits;
        if (LineTraceMultiByChannel(InStart, InEnd, InChannel, InIgnore, hits) <= 0)
            return false;
        OutHit = hits.front();
        return true;
    }

    int32_t FSimplePhysicsScene::LineTraceMultiByChannel(const glm::vec3& InStart, const glm::vec3& InEnd,
                                                         ECollisionChannel InChannel, AActor* InIgnore,
                                                         std::vector<FHitResult>& OutHits) const {
        OutHits.clear();
        const glm::vec3 delta = InEnd - InStart;
        const float len = glm::length(delta);
        if (len < 1e-8f)
            return 0;
        const glm::vec3 dir = delta / len;
        std::vector<FColliderDesc> colliders;
        CollectColliders(InIgnore, colliders);
        for (const auto& c : colliders) {
            if (!ColliderRespondsToChannel(c, InChannel))
                continue;
            FHitResult hit;
            if (!RaycastCollider(c, InStart, dir, len, hit))
                continue;
            hit.Channel = InChannel;
            hit.Time = hit.Distance / len;
            OutHits.push_back(hit);
        }
        std::sort(OutHits.begin(), OutHits.end(),
                  [](const FHitResult& a, const FHitResult& b) { return a.Distance < b.Distance; });
        return static_cast<int32_t>(OutHits.size());
    }

    bool FSimplePhysicsScene::SweepSingleByChannel(const glm::vec3& InStart, const glm::vec3& InEnd, float InRadius,
                                                   ECollisionChannel InChannel, AActor* InIgnore,
                                                   FHitResult& OutHit) const {
        OutHit = {};
        std::vector<FHitResult> hits;
        if (SweepMultiByChannel(InStart, InEnd, InRadius, InChannel, InIgnore, hits) <= 0)
            return false;
        OutHit = hits.front();
        return true;
    }

    int32_t FSimplePhysicsScene::SweepMultiByChannel(const glm::vec3& InStart, const glm::vec3& InEnd, float InRadius,
                                                     ECollisionChannel InChannel, AActor* InIgnore,
                                                     std::vector<FHitResult>& OutHits) const {
        OutHits.clear();
        std::vector<FColliderDesc> colliders;
        CollectColliders(InIgnore, colliders);
        const float len = glm::length(InEnd - InStart);
        for (const auto& c : colliders) {
            if (!ColliderRespondsToChannel(c, InChannel))
                continue;
            FHitResult hit;
            if (!SweepSphereCollider(c, InStart, InEnd, InRadius, hit))
                continue;
            hit.Channel = InChannel;
            if (len > 1e-8f)
                hit.Time = hit.Distance / len;
            OutHits.push_back(hit);
        }
        std::sort(OutHits.begin(), OutHits.end(),
                  [](const FHitResult& a, const FHitResult& b) { return a.Distance < b.Distance; });
        return static_cast<int32_t>(OutHits.size());
    }

    bool FSimplePhysicsScene::SweepCapsuleSingleByChannel(const glm::vec3& InStart, const glm::vec3& InEnd,
                                                          float InRadius, float InHalfHeight,
                                                          ECollisionChannel InChannel, AActor* InIgnore,
                                                          FHitResult& OutHit) const {
        OutHit = {};
        std::vector<FHitResult> hits;
        if (SweepCapsuleMultiByChannel(InStart, InEnd, InRadius, InHalfHeight, InChannel, InIgnore, hits) <= 0)
            return false;
        OutHit = hits.front();
        return true;
    }

    int32_t FSimplePhysicsScene::SweepCapsuleMultiByChannel(const glm::vec3& InStart, const glm::vec3& InEnd,
                                                            float InRadius, float InHalfHeight,
                                                            ECollisionChannel InChannel, AActor* InIgnore,
                                                            std::vector<FHitResult>& OutHits) const {
        // Approximate Y-up capsule as three sphere sweeps (center + hemisphere centers).
        OutHits.clear();
        const float radius = std::max(InRadius, 0.01f);
        const float cyl = std::max(InHalfHeight - radius, 0.0f);
        const glm::vec3 offsets[3] = {{0.0f, 0.0f, 0.0f}, {0.0f, cyl, 0.0f}, {0.0f, -cyl, 0.0f}};
        for (const glm::vec3& off : offsets) {
            std::vector<FHitResult> slice;
            SweepMultiByChannel(InStart + off, InEnd + off, radius, InChannel, InIgnore, slice);
            OutHits.insert(OutHits.end(), slice.begin(), slice.end());
        }
        std::sort(OutHits.begin(), OutHits.end(),
                  [](const FHitResult& a, const FHitResult& b) { return a.Distance < b.Distance; });
        // Deduplicate by actor+component keeping earliest hit.
        std::vector<FHitResult> unique;
        unique.reserve(OutHits.size());
        for (const FHitResult& hit : OutHits) {
            bool bFound = false;
            for (FHitResult& existing : unique) {
                if (existing.Actor == hit.Actor && existing.Component == hit.Component) {
                    bFound = true;
                    break;
                }
            }
            if (!bFound)
                unique.push_back(hit);
        }
        OutHits = std::move(unique);
        return static_cast<int32_t>(OutHits.size());
    }

    bool FSimplePhysicsScene::OverlapAnyTestByChannel(const glm::vec3& InPos, const glm::vec3& InHalfExtent,
                                                      ECollisionChannel InChannel, AActor* InIgnore) const {
        std::vector<FHitResult> hits;
        return OverlapMultiByChannel(InPos, InHalfExtent, InChannel, InIgnore, hits) > 0;
    }

    int32_t FSimplePhysicsScene::OverlapMultiByChannel(const glm::vec3& InPos, const glm::vec3& InHalfExtent,
                                                       ECollisionChannel InChannel, AActor* InIgnore,
                                                       std::vector<FHitResult>& OutHits) const {
        OutHits.clear();
        glm::vec3 minB = InPos - InHalfExtent;
        glm::vec3 maxB = InPos + InHalfExtent;
        std::vector<FColliderDesc> colliders;
        CollectColliders(InIgnore, colliders);
        for (const auto& c : colliders) {
            if (!ColliderRespondsToChannel(c, InChannel))
                continue;
            FHitResult hit;
            if (!OverlapAABBCollider(c, minB, maxB, hit))
                continue;
            hit.Channel = InChannel;
            OutHits.push_back(hit);
        }
        return static_cast<int32_t>(OutHits.size());
    }

} // namespace Leon
