#include "Physics/FSimplePhysicsScene.hpp"
#include "Physics/FCollisionQuery.hpp"
#include "Engine/UWorld.hpp"
#include "Gameplay/AActor.hpp"
#include "Gameplay/ACharacter.hpp"
#include "Gameplay/UPrimitiveComponent.hpp"

#include <algorithm>

namespace Leon {

    void FSimplePhysicsBody::SetTransform(const glm::vec3& InLocation, const glm::quat& InRotation) {
        Location = InLocation;
        Rotation = InRotation;
        // Kinematic sync is component → body. Writing the actor here teleports pawns
        // to the capsule centre (relative offset) and pins them inside the floor.
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

    void FSimplePhysicsScene::CollectColliders(AActor* InIgnore, std::vector<FColliderDesc>& Out) const {
        if (World)
            GatherWorldColliders(*World, InIgnore, Out);
        for (const auto& body : Bodies) {
            if (!body)
                continue;
            if (InIgnore && body->Info.Actor == InIgnore)
                continue;
            if (body->Info.Actor)
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
            d.CollisionEnabled = ECollisionEnabled::QueryAndPhysics;
            Out.push_back(d);
        }
    }

    void FSimplePhysicsScene::Tick(float InDeltaSeconds) {
        const glm::vec3 gravity(0.0f, -22.0f, 0.0f);
        for (auto& body : Bodies) {
            if (!body || !body->bSimulating)
                continue;
            if (body->Info.bEnableGravity)
                body->LinearVelocity += gravity * InDeltaSeconds;
            body->LinearVelocity += body->PendingForce * InDeltaSeconds / std::max(body->Info.Mass, 0.001f);
            body->LinearVelocity += body->PendingImpulse / std::max(body->Info.Mass, 0.001f);
            body->PendingForce = glm::vec3(0.0f);
            body->PendingImpulse = glm::vec3(0.0f);
            body->LinearVelocity *= std::max(0.0f, 1.0f - body->Info.LinearDamping * InDeltaSeconds);
            // Cap runaway velocities — simple dynamics have no world collision on XZ.
            const float speed = glm::length(body->LinearVelocity);
            constexpr float kMaxSimSpeed = 14.0f;
            if (speed > kMaxSimSpeed)
                body->LinearVelocity *= kMaxSimSpeed / speed;
            body->Location += body->LinearVelocity * InDeltaSeconds;
            if (body->Info.Actor) {
                glm::vec3 actorLoc = body->Location;
                glm::vec3 relative(0.0f);
                if (auto* prim = dynamic_cast<UPrimitiveComponent*>(body->Info.Component))
                    relative = prim->GetRelativeLocation();
                actorLoc -= relative;
                // Simple dynamics have no world collision; keep ragdolls on the character floor.
                if (auto* character = dynamic_cast<ACharacter*>(body->Info.Actor)) {
                    const float minActorY = character->GetFloorZ() + character->GetCapsuleHalfHeight();
                    if (actorLoc.y < minActorY) {
                        actorLoc.y = minActorY;
                        body->Location = actorLoc + relative;
                        body->LinearVelocity.y = std::max(0.0f, body->LinearVelocity.y);
                        body->LinearVelocity.x *= 0.35f;
                        body->LinearVelocity.z *= 0.35f;
                    }
                    // Soft arena clamp so dead bodies do not leave the playable volume.
                    constexpr float kArenaHalf = 22.0f;
                    actorLoc.x = std::clamp(actorLoc.x, -kArenaHalf, kArenaHalf);
                    actorLoc.z = std::clamp(actorLoc.z, -kArenaHalf, kArenaHalf);
                    body->Location = actorLoc + relative;
                }
                body->Info.Actor->SetActorLocation(actorLoc);
            }
        }
    }

    IPhysicsBody* FSimplePhysicsScene::CreateRigidBody(const FPhysicsBodyCreateInfo& InInfo) {
        auto body = std::make_unique<FSimplePhysicsBody>();
        body->Info = InInfo;
        body->Location = InInfo.Location;
        body->Rotation = InInfo.Rotation;
        body->bSimulating = InInfo.bSimulatePhysics && InInfo.Motion == EPhysicsMotionType::Dynamic;
        IPhysicsBody* raw = body.get();
        Bodies.push_back(std::move(body));
        return raw;
    }

    void FSimplePhysicsScene::DestroyRigidBody(IPhysicsBody* InBody) {
        Bodies.erase(
            std::remove_if(Bodies.begin(), Bodies.end(),
                           [InBody](const std::unique_ptr<FSimplePhysicsBody>& b) { return b.get() == InBody; }),
            Bodies.end());
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
        for (const auto& c : colliders) {
            if (!ColliderRespondsToChannel(c, InChannel))
                continue;
            FHitResult hit;
            if (!SweepSphereCollider(c, InStart, InEnd, InRadius, hit))
                continue;
            hit.Channel = InChannel;
            OutHits.push_back(hit);
        }
        std::sort(OutHits.begin(), OutHits.end(),
                  [](const FHitResult& a, const FHitResult& b) { return a.Distance < b.Distance; });
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
