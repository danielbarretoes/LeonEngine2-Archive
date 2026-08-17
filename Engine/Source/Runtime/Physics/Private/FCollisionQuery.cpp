#include "Physics/FCollisionQuery.hpp"
#include "Engine/UWorld.hpp"
#include "Engine/Components.hpp"
#include "Gameplay/AActor.hpp"
#include "Gameplay/UPrimitiveComponent.hpp"
#include "Assets/UStaticMesh.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Leon {

    namespace {

        bool AABBsOverlap(const glm::vec3& AMin, const glm::vec3& AMax, const glm::vec3& BMin, const glm::vec3& BMax) {
            return AMin.x <= BMax.x && AMax.x >= BMin.x && AMin.y <= BMax.y && AMax.y >= BMin.y && AMin.z <= BMax.z &&
                   AMax.z >= BMin.z;
        }

        void TransformAABBCorners(const glm::vec3& InLocalMin, const glm::vec3& InLocalMax, const glm::mat4& InWorld,
                                  glm::vec3& OutMin, glm::vec3& OutMax) {
            OutMin = glm::vec3(std::numeric_limits<float>::max());
            OutMax = glm::vec3(-std::numeric_limits<float>::max());
            const glm::vec3 corners[8] = {
                {InLocalMin.x, InLocalMin.y, InLocalMin.z}, {InLocalMax.x, InLocalMin.y, InLocalMin.z},
                {InLocalMin.x, InLocalMax.y, InLocalMin.z}, {InLocalMax.x, InLocalMax.y, InLocalMin.z},
                {InLocalMin.x, InLocalMin.y, InLocalMax.z}, {InLocalMax.x, InLocalMin.y, InLocalMax.z},
                {InLocalMin.x, InLocalMax.y, InLocalMax.z}, {InLocalMax.x, InLocalMax.y, InLocalMax.z},
            };
            for (const glm::vec3& c : corners) {
                glm::vec3 w = glm::vec3(InWorld * glm::vec4(c, 1.0f));
                OutMin = glm::min(OutMin, w);
                OutMax = glm::max(OutMax, w);
            }
        }

        bool RayAABB(const glm::vec3& InStart, const glm::vec3& InInvDir, float InLen, const glm::vec3& InMin,
                     const glm::vec3& InMax, glm::vec3 InDir, float& OutT, glm::vec3& OutNormal) {
            float tmin = 0.0f;
            float tmax = InLen;
            glm::vec3 hitNormal{0.0f, 1.0f, 0.0f};
            for (int axis = 0; axis < 3; ++axis) {
                if (std::abs(InDir[axis]) < 1e-8f) {
                    if (InStart[axis] < InMin[axis] || InStart[axis] > InMax[axis])
                        return false;
                    continue;
                }
                float origin = InStart[axis];
                float inv = InInvDir[axis];
                float t0 = (InMin[axis] - origin) * inv;
                float t1 = (InMax[axis] - origin) * inv;
                glm::vec3 n0(0.0f);
                glm::vec3 n1(0.0f);
                n0[axis] = -1.0f;
                n1[axis] = 1.0f;
                if (inv < 0.0f) {
                    std::swap(t0, t1);
                    std::swap(n0, n1);
                }
                if (t0 > tmin) {
                    tmin = t0;
                    hitNormal = n0;
                }
                tmax = std::min(tmax, t1);
                if (tmax < tmin)
                    return false;
            }
            if (tmin < 0.0f || tmin > InLen)
                return false;
            OutT = tmin;
            OutNormal = hitNormal;
            return true;
        }

        bool RaySphere(const glm::vec3& InStart, const glm::vec3& InDir, float InLen, const glm::vec3& InCenter,
                       float InRadius, float& OutT, glm::vec3& OutNormal) {
            glm::vec3 m = InStart - InCenter;
            float b = glm::dot(m, InDir);
            float c = glm::dot(m, m) - InRadius * InRadius;
            if (c > 0.0f && b > 0.0f)
                return false;
            float discr = b * b - c;
            if (discr < 0.0f)
                return false;
            float t = -b - std::sqrt(discr);
            if (t < 0.0f)
                t = -b + std::sqrt(discr);
            if (t < 0.0f || t > InLen)
                return false;
            OutT = t;
            glm::vec3 p = InStart + InDir * t;
            OutNormal = glm::normalize(p - InCenter);
            return true;
        }

        glm::vec3 ClosestPointOnSegment(const glm::vec3& InP, const glm::vec3& InA, const glm::vec3& InB) {
            glm::vec3 ab = InB - InA;
            float t = glm::dot(InP - InA, ab) / std::max(glm::dot(ab, ab), 1e-8f);
            t = std::clamp(t, 0.0f, 1.0f);
            return InA + ab * t;
        }

        bool RayCapsule(const glm::vec3& InStart, const glm::vec3& InDir, float InLen, const glm::vec3& InCenter,
                        float InRadius, float InHalfHeight, float& OutT, glm::vec3& OutNormal) {
            const float cyl = std::max(InHalfHeight - InRadius, 0.0f);
            glm::vec3 a = InCenter + glm::vec3(0.0f, -cyl, 0.0f);
            glm::vec3 b = InCenter + glm::vec3(0.0f, cyl, 0.0f);
            // Expand as sphere sweep along the capsule axis by testing a thickened segment.
            glm::vec3 ba = b - a;
            glm::vec3 oa = InStart - a;
            float baba = glm::dot(ba, ba);
            float bard = glm::dot(ba, InDir);
            float baoa = glm::dot(ba, oa);
            float rdoa = glm::dot(InDir, oa);
            float oaoa = glm::dot(oa, oa);
            float a2 = baba - bard * bard;
            float b2 = baba * rdoa - baoa * bard;
            float c2 = baba * oaoa - baoa * baoa - InRadius * InRadius * baba;
            float h = b2 * b2 - a2 * c2;
            if (std::abs(a2) < 1e-8f) {
                bool s0 = RaySphere(InStart, InDir, InLen, a, InRadius, OutT, OutNormal);
                float t1;
                glm::vec3 n1;
                bool s1 = RaySphere(InStart, InDir, InLen, b, InRadius, t1, n1);
                if (s1 && (!s0 || t1 < OutT)) {
                    OutT = t1;
                    OutNormal = n1;
                    s0 = true;
                }
                return s0;
            }
            if (h < 0.0f)
                return false;
            float t = (-b2 - std::sqrt(h)) / a2;
            float y = baoa + t * bard;
            if (y > 0.0f && y < baba && t >= 0.0f && t <= InLen) {
                OutT = t;
                glm::vec3 p = InStart + InDir * t;
                OutNormal = glm::normalize(p - ClosestPointOnSegment(p, a, b));
                return true;
            }
            bool s0 = RaySphere(InStart, InDir, InLen, a, InRadius, OutT, OutNormal);
            float t1;
            glm::vec3 n1;
            bool s1 = RaySphere(InStart, InDir, InLen, b, InRadius, t1, n1);
            if (s1 && (!s0 || t1 < OutT)) {
                OutT = t1;
                OutNormal = n1;
                return true;
            }
            return s0;
        }

        void FillHit(FHitResult& OutHit, const FColliderDesc& InCollider, const glm::vec3& InStart,
                     const glm::vec3& InDir, float InT, const glm::vec3& InNormal, ECollisionChannel InChannel) {
            OutHit.bBlockingHit = true;
            OutHit.Actor = InCollider.Actor;
            OutHit.Component = InCollider.Component;
            OutHit.Distance = InT;
            OutHit.Time = InT;
            OutHit.Location = InStart + InDir * InT;
            OutHit.ImpactPoint = OutHit.Location;
            OutHit.Normal = InNormal;
            OutHit.ImpactNormal = InNormal;
            OutHit.Channel = InChannel;
        }

        FColliderDesc MakeBoxDesc(AActor* InActor, const glm::vec3& InWorldMin, const glm::vec3& InWorldMax,
                                  ECollisionChannel InType) {
            FColliderDesc d;
            d.Actor = InActor;
            d.Shape = EPhysicsShapeType::Box;
            d.Center = (InWorldMin + InWorldMax) * 0.5f;
            d.BoxHalfExtent = (InWorldMax - InWorldMin) * 0.5f;
            d.ObjectType = InType;
            d.Responses.Set(ECollisionChannel::Visibility, ECollisionResponse::Block);
            d.Responses.Set(ECollisionChannel::Camera, ECollisionResponse::Block);
            d.Responses.Set(ECollisionChannel::WorldStatic, ECollisionResponse::Block);
            d.Responses.Set(ECollisionChannel::WorldDynamic, ECollisionResponse::Block);
            d.Responses.Set(ECollisionChannel::Pawn, ECollisionResponse::Block);
            d.CollisionEnabled = ECollisionEnabled::QueryAndPhysics;
            return d;
        }

    } // namespace

    bool ColliderRespondsToChannel(const FColliderDesc& InCollider, ECollisionChannel InQuery) {
        if (InCollider.CollisionEnabled == ECollisionEnabled::NoCollision ||
            InCollider.CollisionEnabled == ECollisionEnabled::PhysicsOnly)
            return false;
        if (InCollider.Component)
            return InCollider.Responses.Get(InQuery) == ECollisionResponse::Block;
        return TraceChannelAccepts(InQuery, InCollider.ObjectType);
    }

    void ColliderWorldAABB(const FColliderDesc& InCollider, glm::vec3& OutMin, glm::vec3& OutMax) {
        if (InCollider.Shape == EPhysicsShapeType::Sphere) {
            glm::vec3 e(InCollider.SphereRadius);
            OutMin = InCollider.Center - e;
            OutMax = InCollider.Center + e;
            return;
        }
        if (InCollider.Shape == EPhysicsShapeType::Capsule) {
            glm::vec3 e(InCollider.CapsuleRadius, InCollider.CapsuleHalfHeight, InCollider.CapsuleRadius);
            OutMin = InCollider.Center - e;
            OutMax = InCollider.Center + e;
            return;
        }
        OutMin = InCollider.Center - InCollider.BoxHalfExtent;
        OutMax = InCollider.Center + InCollider.BoxHalfExtent;
    }

    bool GatherActorColliders(AActor& InActor, std::vector<FColliderDesc>& OutColliders) {
        bool bAny = false;
        for (const auto& comp : InActor.GetActorComponents()) {
            auto prim = std::dynamic_pointer_cast<UPrimitiveComponent>(comp);
            if (!prim)
                continue;
            if (prim->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
                continue;
            FPhysicsBodyCreateInfo info = prim->MakeBodyCreateInfo();
            FColliderDesc d;
            d.Actor = &InActor;
            d.Component = prim.get();
            d.Shape = info.Shape;
            d.Center = info.Location;
            d.BoxHalfExtent = info.BoxHalfExtent;
            d.SphereRadius = info.SphereRadius;
            d.CapsuleRadius = info.CapsuleRadius;
            d.CapsuleHalfHeight = info.CapsuleHalfHeight;
            d.ObjectType = info.ObjectType;
            d.Responses = info.Responses;
            d.CollisionEnabled = prim->GetCollisionEnabled();
            OutColliders.push_back(d);
            bAny = true;
        }
        if (bAny)
            return true;

        if (!InActor.HasComponent<FTransformComponent>())
            return false;
        const glm::mat4 world = InActor.GetComponent<FTransformComponent>().GetTransform();

        if (InActor.HasComponent<FBoxCollisionComponent>()) {
            const auto& box = InActor.GetComponent<FBoxCollisionComponent>();
            if (!box.bBlockMovement)
                return false;
            glm::vec3 minB, maxB;
            TransformAABBCorners(box.LocalMin, box.LocalMax, world, minB, maxB);
            OutColliders.push_back(MakeBoxDesc(&InActor, minB, maxB, box.Channel));
            return true;
        }

        if (InActor.HasComponent<UStaticMeshComponent>()) {
            const auto& smc = InActor.GetComponent<UStaticMeshComponent>();
            if (smc.Mobility != EComponentMobility::Static || !smc.StaticMesh)
                return false;
            glm::vec3 minB, maxB;
            TransformAABBCorners(smc.StaticMesh->GetBoundsMin(), smc.StaticMesh->GetBoundsMax(), world, minB, maxB);
            OutColliders.push_back(MakeBoxDesc(&InActor, minB, maxB, ECollisionChannel::WorldStatic));
            return true;
        }

        if (InActor.HasComponent<FMeshComponent>()) {
            const auto& mesh = InActor.GetComponent<FMeshComponent>();
            if (mesh.Mobility != EComponentMobility::Static)
                return false;
            glm::vec3 localMin(-0.5f);
            glm::vec3 localMax(0.5f);
            if (mesh.MeshType == "Cube") {
                float h = mesh.MeshSize * 0.5f;
                localMin = glm::vec3(-h);
                localMax = glm::vec3(h);
            } else if (mesh.MeshType == "Plane") {
                localMin = glm::vec3(-mesh.MeshWidth * 0.5f, -0.05f, -mesh.MeshDepth * 0.5f);
                localMax = glm::vec3(mesh.MeshWidth * 0.5f, 0.05f, mesh.MeshDepth * 0.5f);
            } else if (mesh.MeshType == "Sphere") {
                localMin = glm::vec3(-mesh.MeshRadius);
                localMax = glm::vec3(mesh.MeshRadius);
            } else if (mesh.MeshType == "Cylinder" || mesh.MeshType == "Cone") {
                localMin = glm::vec3(-mesh.MeshRadius, -mesh.MeshHeight * 0.5f, -mesh.MeshRadius);
                localMax = glm::vec3(mesh.MeshRadius, mesh.MeshHeight * 0.5f, mesh.MeshRadius);
            } else {
                localMin = glm::vec3(-mesh.MeshWidth * 0.5f, -mesh.MeshHeight * 0.5f, -mesh.MeshDepth * 0.5f);
                localMax = glm::vec3(mesh.MeshWidth * 0.5f, mesh.MeshHeight * 0.5f, mesh.MeshDepth * 0.5f);
            }
            glm::vec3 minB, maxB;
            TransformAABBCorners(localMin, localMax, world, minB, maxB);
            OutColliders.push_back(MakeBoxDesc(&InActor, minB, maxB, ECollisionChannel::WorldStatic));
            return true;
        }
        return false;
    }

    void GatherWorldColliders(UWorld& InWorld, AActor* InIgnore, std::vector<FColliderDesc>& OutColliders) {
        for (const auto& actorRef : InWorld.GetAllActors()) {
            if (!actorRef || actorRef.get() == InIgnore || actorRef->IsPendingKill())
                continue;
            GatherActorColliders(*actorRef, OutColliders);
        }
    }

    bool RaycastCollider(const FColliderDesc& InCollider, const glm::vec3& InStart, const glm::vec3& InDir, float InLen,
                         FHitResult& OutHit) {
        float t = 0.0f;
        glm::vec3 n{0.0f, 1.0f, 0.0f};
        bool bHit = false;
        if (InCollider.Shape == EPhysicsShapeType::Sphere) {
            bHit = RaySphere(InStart, InDir, InLen, InCollider.Center, InCollider.SphereRadius, t, n);
        } else if (InCollider.Shape == EPhysicsShapeType::Capsule) {
            bHit = RayCapsule(InStart, InDir, InLen, InCollider.Center, InCollider.CapsuleRadius,
                              InCollider.CapsuleHalfHeight, t, n);
        } else {
            glm::vec3 minB, maxB;
            ColliderWorldAABB(InCollider, minB, maxB);
            glm::vec3 inv(InDir.x != 0.0f ? 1.0f / InDir.x : 1e30f, InDir.y != 0.0f ? 1.0f / InDir.y : 1e30f,
                          InDir.z != 0.0f ? 1.0f / InDir.z : 1e30f);
            bHit = RayAABB(InStart, inv, InLen, minB, maxB, InDir, t, n);
        }
        if (!bHit)
            return false;
        FillHit(OutHit, InCollider, InStart, InDir, t, n, InCollider.ObjectType);
        return true;
    }

    bool SweepSphereCollider(const FColliderDesc& InCollider, const glm::vec3& InStart, const glm::vec3& InEnd,
                             float InRadius, FHitResult& OutHit) {
        glm::vec3 minB, maxB;
        ColliderWorldAABB(InCollider, minB, maxB);
        minB -= glm::vec3(InRadius);
        maxB += glm::vec3(InRadius);
        glm::vec3 delta = InEnd - InStart;
        float len = glm::length(delta);
        if (len < 1e-8f) {
            if (AABBsOverlap(InStart - glm::vec3(InRadius), InStart + glm::vec3(InRadius), minB + glm::vec3(InRadius),
                             maxB - glm::vec3(InRadius))) {
                OutHit.bBlockingHit = true;
                OutHit.Actor = InCollider.Actor;
                OutHit.Component = InCollider.Component;
                OutHit.Location = InStart;
                OutHit.ImpactPoint = InStart;
                OutHit.Distance = 0.0f;
                OutHit.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
                OutHit.ImpactNormal = OutHit.Normal;
                OutHit.Channel = InCollider.ObjectType;
                return true;
            }
            return false;
        }
        glm::vec3 dir = delta / len;
        glm::vec3 inv(dir.x != 0.0f ? 1.0f / dir.x : 1e30f, dir.y != 0.0f ? 1.0f / dir.y : 1e30f,
                      dir.z != 0.0f ? 1.0f / dir.z : 1e30f);
        float t = 0.0f;
        glm::vec3 n{0, 1, 0};
        if (!RayAABB(InStart, inv, len, minB, maxB, dir, t, n))
            return false;
        FillHit(OutHit, InCollider, InStart, dir, t, n, InCollider.ObjectType);
        return true;
    }

    bool OverlapAABBCollider(const FColliderDesc& InCollider, const glm::vec3& InMin, const glm::vec3& InMax,
                             FHitResult& OutHit) {
        glm::vec3 minB, maxB;
        ColliderWorldAABB(InCollider, minB, maxB);
        if (!AABBsOverlap(InMin, InMax, minB, maxB))
            return false;
        OutHit.bBlockingHit = true;
        OutHit.Actor = InCollider.Actor;
        OutHit.Component = InCollider.Component;
        OutHit.Location = (glm::max(InMin, minB) + glm::min(InMax, maxB)) * 0.5f;
        OutHit.ImpactPoint = OutHit.Location;
        glm::vec3 overlap = glm::min(InMax, maxB) - glm::max(InMin, minB);
        if (overlap.x <= overlap.y && overlap.x <= overlap.z)
            OutHit.Normal = (InMin.x + InMax.x < minB.x + maxB.x) ? glm::vec3(-1, 0, 0) : glm::vec3(1, 0, 0);
        else if (overlap.y <= overlap.z)
            OutHit.Normal = (InMin.y + InMax.y < minB.y + maxB.y) ? glm::vec3(0, -1, 0) : glm::vec3(0, 1, 0);
        else
            OutHit.Normal = (InMin.z + InMax.z < minB.z + maxB.z) ? glm::vec3(0, 0, -1) : glm::vec3(0, 0, 1);
        OutHit.ImpactNormal = OutHit.Normal;
        OutHit.Channel = InCollider.ObjectType;
        return true;
    }

} // namespace Leon
