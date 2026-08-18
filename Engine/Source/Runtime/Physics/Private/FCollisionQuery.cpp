#include "Physics/FCollisionQuery.hpp"
#include "Engine/UWorld.hpp"
#include "Engine/Components.hpp"
#include "Gameplay/AActor.hpp"
#include "Gameplay/UPrimitiveComponent.hpp"
#include "Assets/UStaticMesh.hpp"
#include "Renderer/FDebugRenderer.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Leon {

    namespace {

        constexpr float kTriEps = 1e-7f;

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

        glm::vec3 TransformPoint(const glm::mat4& InWorld, const glm::vec3& InP) {
            return glm::vec3(InWorld * glm::vec4(InP, 1.0f));
        }

        glm::vec3 ClosestPointOnTriangle(const glm::vec3& InP, const glm::vec3& InA, const glm::vec3& InB,
                                         const glm::vec3& InC) {
            const glm::vec3 ab = InB - InA;
            const glm::vec3 ac = InC - InA;
            const glm::vec3 ap = InP - InA;
            const float d1 = glm::dot(ab, ap);
            const float d2 = glm::dot(ac, ap);
            if (d1 <= 0.0f && d2 <= 0.0f)
                return InA;
            const glm::vec3 bp = InP - InB;
            const float d3 = glm::dot(ab, bp);
            const float d4 = glm::dot(ac, bp);
            if (d3 >= 0.0f && d4 <= d3)
                return InB;
            const float vc = d1 * d4 - d3 * d2;
            if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
                const float v = d1 / (d1 - d3);
                return InA + v * ab;
            }
            const glm::vec3 cp = InP - InC;
            const float d5 = glm::dot(ab, cp);
            const float d6 = glm::dot(ac, cp);
            if (d6 >= 0.0f && d5 <= d6)
                return InC;
            const float vb = d5 * d2 - d1 * d6;
            if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
                const float w = d2 / (d2 - d6);
                return InA + w * ac;
            }
            const float va = d3 * d6 - d5 * d4;
            if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
                const float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
                return InB + w * (InC - InB);
            }
            const float denom = 1.0f / (va + vb + vc);
            return InA + ab * (vb * denom) + ac * (vc * denom);
        }

        bool RayTriangle(const glm::vec3& InStart, const glm::vec3& InDir, float InLen, const glm::vec3& InA,
                         const glm::vec3& InB, const glm::vec3& InC, float& OutT, glm::vec3& OutNormal) {
            const glm::vec3 e1 = InB - InA;
            const glm::vec3 e2 = InC - InA;
            const glm::vec3 p = glm::cross(InDir, e2);
            const float det = glm::dot(e1, p);
            if (std::abs(det) < kTriEps)
                return false;
            const float inv = 1.0f / det;
            const glm::vec3 tvec = InStart - InA;
            const float u = glm::dot(tvec, p) * inv;
            if (u < 0.0f || u > 1.0f)
                return false;
            const glm::vec3 q = glm::cross(tvec, e1);
            const float v = glm::dot(InDir, q) * inv;
            if (v < 0.0f || u + v > 1.0f)
                return false;
            const float t = glm::dot(e2, q) * inv;
            if (t < 0.0f || t > InLen)
                return false;
            OutT = t;
            OutNormal = glm::normalize(glm::cross(e1, e2));
            if (glm::dot(OutNormal, InDir) > 0.0f)
                OutNormal = -OutNormal;
            return true;
        }

        bool TriangleOverlapsAABB(const glm::vec3& InA, const glm::vec3& InB, const glm::vec3& InC,
                                  const glm::vec3& InMin, const glm::vec3& InMax) {
            const glm::vec3 triMin = glm::min(InA, glm::min(InB, InC));
            const glm::vec3 triMax = glm::max(InA, glm::max(InB, InC));
            if (!AABBsOverlap(triMin, triMax, InMin, InMax))
                return false;
            const glm::vec3 center = (InMin + InMax) * 0.5f;
            const glm::vec3 ext = (InMax - InMin) * 0.5f;
            const glm::vec3 v0 = InA - center;
            const glm::vec3 v1 = InB - center;
            const glm::vec3 v2 = InC - center;
            const glm::vec3 e0 = v1 - v0;
            const glm::vec3 e1 = v2 - v1;
            const glm::vec3 e2 = v0 - v2;
            auto axisTest = [&](const glm::vec3& axis) {
                const float r = ext.x * std::abs(axis.x) + ext.y * std::abs(axis.y) + ext.z * std::abs(axis.z);
                const float p0 = glm::dot(v0, axis);
                const float p1 = glm::dot(v1, axis);
                const float p2 = glm::dot(v2, axis);
                const float mn = std::min(p0, std::min(p1, p2));
                const float mx = std::max(p0, std::max(p1, p2));
                return mn > r || mx < -r;
            };
            if (axisTest(glm::cross(e0, glm::vec3(1, 0, 0))) || axisTest(glm::cross(e0, glm::vec3(0, 1, 0))) ||
                axisTest(glm::cross(e0, glm::vec3(0, 0, 1))) || axisTest(glm::cross(e1, glm::vec3(1, 0, 0))) ||
                axisTest(glm::cross(e1, glm::vec3(0, 1, 0))) || axisTest(glm::cross(e1, glm::vec3(0, 0, 1))) ||
                axisTest(glm::cross(e2, glm::vec3(1, 0, 0))) || axisTest(glm::cross(e2, glm::vec3(0, 1, 0))) ||
                axisTest(glm::cross(e2, glm::vec3(0, 0, 1))))
                return false;
            const glm::vec3 n = glm::cross(e0, e1);
            if (axisTest(n))
                return false;
            return true;
        }

        template <typename Fn> void ForEachWorldTriangle(const FColliderDesc& InCollider, Fn&& InFn) {
            if (!InCollider.TriangleMesh)
                return;
            const auto& verts = InCollider.TriangleMesh->GetVertices();
            const auto& indices = InCollider.TriangleMesh->GetIndices();
            const glm::mat4& w = InCollider.TriangleWorld;
            const size_t triCount = indices.size() / 3;
            for (size_t t = 0; t < triCount; ++t) {
                const glm::vec3 a = TransformPoint(w, verts[indices[t * 3 + 0]].Position);
                const glm::vec3 b = TransformPoint(w, verts[indices[t * 3 + 1]].Position);
                const glm::vec3 c = TransformPoint(w, verts[indices[t * 3 + 2]].Position);
                if (!InFn(a, b, c))
                    return;
            }
        }

        bool RaycastTriangleMesh(const FColliderDesc& InCollider, const glm::vec3& InStart, const glm::vec3& InDir,
                                 float InLen, float& OutT, glm::vec3& OutNormal) {
            bool bHit = false;
            float best = InLen;
            glm::vec3 bestN{0, 1, 0};
            ForEachWorldTriangle(InCollider, [&](const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) {
                float t = 0.0f;
                glm::vec3 n{0, 1, 0};
                if (RayTriangle(InStart, InDir, best, a, b, c, t, n) && t < best) {
                    best = t;
                    bestN = n;
                    bHit = true;
                }
                return true;
            });
            if (!bHit)
                return false;
            OutT = best;
            OutNormal = bestN;
            return true;
        }

        bool SweepSphereTriangleMesh(const FColliderDesc& InCollider, const glm::vec3& InStart, const glm::vec3& InEnd,
                                     float InRadius, float& OutT, glm::vec3& OutNormal) {
            glm::vec3 delta = InEnd - InStart;
            float len = glm::length(delta);
            glm::vec3 dir = len > 1e-8f ? delta / len : glm::vec3(0, -1, 0);
            bool bHit = false;
            float best = len + InRadius;
            glm::vec3 bestN{0, 1, 0};

            auto consider = [&](float t, const glm::vec3& n) {
                if (t < 0.0f || t > best)
                    return;
                best = t;
                bestN = n;
                bHit = true;
            };

            ForEachWorldTriangle(InCollider, [&](const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) {
                const glm::vec3 closest = ClosestPointOnTriangle(InStart, a, b, c);
                const glm::vec3 toP = InStart - closest;
                const float dist2 = glm::dot(toP, toP);
                if (dist2 <= InRadius * InRadius) {
                    consider(0.0f, glm::length(toP) > kTriEps ? glm::normalize(toP) : glm::vec3(0, 1, 0));
                    return true;
                }
                glm::vec3 n = glm::normalize(glm::cross(b - a, c - a));
                if (glm::dot(n, dir) > 0.0f)
                    n = -n;
                float tPlane = 0.0f;
                glm::vec3 hitN;
                if (RayTriangle(InStart, dir, len + InRadius, a + n * InRadius, b + n * InRadius, c + n * InRadius,
                                tPlane, hitN))
                    consider(tPlane, n);

                const glm::vec3 verts[3] = {a, b, c};
                const glm::vec3 edges[3][2] = {{a, b}, {b, c}, {c, a}};
                for (const auto& e : edges) {
                    glm::vec3 ba = e[1] - e[0];
                    glm::vec3 oa = InStart - e[0];
                    float baba = glm::dot(ba, ba);
                    float bard = glm::dot(ba, dir);
                    float baoa = glm::dot(ba, oa);
                    float rdoa = glm::dot(dir, oa);
                    float oaoa = glm::dot(oa, oa);
                    float a2 = baba - bard * bard;
                    float b2 = baba * rdoa - baoa * bard;
                    float c2 = baba * oaoa - baoa * baoa - InRadius * InRadius * baba;
                    float h = b2 * b2 - a2 * c2;
                    if (std::abs(a2) > kTriEps && h >= 0.0f) {
                        float t = (-b2 - std::sqrt(h)) / a2;
                        float y = baoa + t * bard;
                        if (y > 0.0f && y < baba && t >= 0.0f)
                            consider(t, glm::normalize(InStart + dir * t - (e[0] + ba * (y / baba))));
                    }
                }
                for (const glm::vec3& v : verts) {
                    glm::vec3 m = InStart - v;
                    float bdot = glm::dot(m, dir);
                    float cdot = glm::dot(m, m) - InRadius * InRadius;
                    if (cdot > 0.0f && bdot > 0.0f)
                        continue;
                    float discr = bdot * bdot - cdot;
                    if (discr < 0.0f)
                        continue;
                    float t = -bdot - std::sqrt(discr);
                    if (t < 0.0f)
                        t = -bdot + std::sqrt(discr);
                    if (t >= 0.0f)
                        consider(t, glm::normalize(InStart + dir * t - v));
                }
                return true;
            });
            if (!bHit)
                return false;
            OutT = best;
            OutNormal = bestN;
            return true;
        }

        bool OverlapAABBTriangleMesh(const FColliderDesc& InCollider, const glm::vec3& InMin, const glm::vec3& InMax,
                                     glm::vec3& OutPoint, glm::vec3& OutNormal, float& OutDepth) {
            bool bHit = false;
            float bestDepth = 0.0f;
            glm::vec3 bestP = (InMin + InMax) * 0.5f;
            glm::vec3 bestN{0, 1, 0};
            const glm::vec3 qCenter = bestP;
            ForEachWorldTriangle(InCollider, [&](const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) {
                if (!TriangleOverlapsAABB(a, b, c, InMin, InMax))
                    return true;
                const glm::vec3 closest = ClosestPointOnTriangle(qCenter, a, b, c);
                glm::vec3 n = glm::normalize(glm::cross(b - a, c - a));
                if (glm::dot(n, qCenter - closest) < 0.0f)
                    n = -n;
                const glm::vec3 half = (InMax - InMin) * 0.5f;
                const float proj = std::abs(n.x) * half.x + std::abs(n.y) * half.y + std::abs(n.z) * half.z;
                const float sep = glm::dot(qCenter - closest, n);
                const float depth = proj - sep;
                if (depth > bestDepth) {
                    bestDepth = depth;
                    bestP = closest;
                    bestN = n;
                    bHit = true;
                }
                return true;
            });
            if (!bHit)
                return false;
            OutPoint = bestP;
            OutNormal = bestN;
            OutDepth = bestDepth;
            return true;
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

        FColliderDesc MakeTriangleMeshDesc(AActor* InActor, const UStaticMesh& InMesh, const glm::mat4& InWorld) {
            FColliderDesc d =
                MakeBoxDesc(InActor, InMesh.GetBoundsMin(), InMesh.GetBoundsMax(), ECollisionChannel::WorldStatic);
            glm::vec3 minB, maxB;
            TransformAABBCorners(InMesh.GetBoundsMin(), InMesh.GetBoundsMax(), InWorld, minB, maxB);
            d.Center = (minB + maxB) * 0.5f;
            d.BoxHalfExtent = (maxB - minB) * 0.5f;
            d.Shape = EPhysicsShapeType::TriangleMesh;
            d.TriangleMesh = &InMesh;
            d.TriangleWorld = InWorld;
            return d;
        }

    } // namespace

    bool ColliderRespondsToChannel(const FColliderDesc& InCollider, ECollisionChannel InQuery) {
        if (InCollider.Component) {
            const ECollisionEnabled enabled = InCollider.Component->GetCollisionEnabled();
            if (enabled == ECollisionEnabled::NoCollision || enabled == ECollisionEnabled::PhysicsOnly)
                return false;
            return InCollider.Component->GetCollisionResponseToChannel(InQuery) == ECollisionResponse::Block;
        }
        if (InCollider.CollisionEnabled == ECollisionEnabled::NoCollision ||
            InCollider.CollisionEnabled == ECollisionEnabled::PhysicsOnly)
            return false;
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

        if (InActor.HasComponent<FStaticMeshComponent>()) {
            const auto& smc = InActor.GetComponent<FStaticMeshComponent>();
            if (smc.Mobility != EComponentMobility::Static || !smc.StaticMesh)
                return false;
            OutColliders.push_back(MakeTriangleMeshDesc(&InActor, *smc.StaticMesh, world));
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

    void DrawDebugWorldColliders(UWorld& InWorld) {
        std::vector<FColliderDesc> colliders;
        GatherWorldColliders(InWorld, nullptr, colliders);
        constexpr float kInflate = 0.02f; // slight expand so coplanar edges remain readable with depth test
        for (const auto& c : colliders) {
            glm::vec4 color(0.15f, 1.0f, 0.35f, 1.0f); // WorldStatic — bright green
            switch (c.ObjectType) {
            case ECollisionChannel::Pawn:
                color = glm::vec4(1.0f, 0.55f, 0.05f, 1.0f); // orange capsule / pawn
                break;
            case ECollisionChannel::Visibility:
                color = glm::vec4(1.0f, 0.95f, 0.2f, 1.0f);
                break;
            case ECollisionChannel::Camera:
                color = glm::vec4(0.3f, 0.75f, 1.0f, 1.0f);
                break;
            case ECollisionChannel::WorldDynamic:
                color = glm::vec4(0.95f, 0.3f, 1.0f, 1.0f);
                break;
            default:
                break;
            }
            switch (c.Shape) {
            case EPhysicsShapeType::Sphere:
                FDebugRenderer::DrawDebugSphere(c.Center, c.SphereRadius + kInflate, color);
                break;
            case EPhysicsShapeType::Capsule:
                FDebugRenderer::DrawDebugCapsule(c.Center, c.CapsuleRadius + kInflate, c.CapsuleHalfHeight + kInflate,
                                                 color);
                break;
            case EPhysicsShapeType::Box:
            default:
                FDebugRenderer::DrawDebugBox(c.Center, c.BoxHalfExtent + glm::vec3(kInflate), color);
                break;
            }
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
        } else if (InCollider.Shape == EPhysicsShapeType::TriangleMesh) {
            glm::vec3 minB, maxB;
            ColliderWorldAABB(InCollider, minB, maxB);
            glm::vec3 inv(InDir.x != 0.0f ? 1.0f / InDir.x : 1e30f, InDir.y != 0.0f ? 1.0f / InDir.y : 1e30f,
                          InDir.z != 0.0f ? 1.0f / InDir.z : 1e30f);
            float aabbT = 0.0f;
            glm::vec3 aabbN{0, 1, 0};
            if (!RayAABB(InStart, inv, InLen, minB, maxB, InDir, aabbT, aabbN))
                return false;
            bHit = RaycastTriangleMesh(InCollider, InStart, InDir, InLen, t, n);
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
        if (InCollider.Shape == EPhysicsShapeType::TriangleMesh) {
            glm::vec3 minB, maxB;
            ColliderWorldAABB(InCollider, minB, maxB);
            minB -= glm::vec3(InRadius);
            maxB += glm::vec3(InRadius);
            glm::vec3 delta = InEnd - InStart;
            float len = glm::length(delta);
            glm::vec3 dir = len > 1e-8f ? delta / len : glm::vec3(0, -1, 0);
            glm::vec3 inv(dir.x != 0.0f ? 1.0f / dir.x : 1e30f, dir.y != 0.0f ? 1.0f / dir.y : 1e30f,
                          dir.z != 0.0f ? 1.0f / dir.z : 1e30f);
            float aabbT = 0.0f;
            glm::vec3 aabbN{0, 1, 0};
            if (len > 1e-8f && !RayAABB(InStart, inv, len + InRadius, minB, maxB, dir, aabbT, aabbN) &&
                !AABBsOverlap(InStart - glm::vec3(InRadius), InStart + glm::vec3(InRadius), minB, maxB))
                return false;
            float t = 0.0f;
            glm::vec3 n{0, 1, 0};
            if (!SweepSphereTriangleMesh(InCollider, InStart, InEnd, InRadius, t, n))
                return false;
            FillHit(OutHit, InCollider, InStart, dir, t, n, InCollider.ObjectType);
            return true;
        }
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
        if (InCollider.Shape == EPhysicsShapeType::TriangleMesh) {
            glm::vec3 minB, maxB;
            ColliderWorldAABB(InCollider, minB, maxB);
            if (!AABBsOverlap(InMin, InMax, minB, maxB))
                return false;
            glm::vec3 p, n;
            float depth = 0.0f;
            if (!OverlapAABBTriangleMesh(InCollider, InMin, InMax, p, n, depth))
                return false;
            OutHit.bBlockingHit = true;
            OutHit.Actor = InCollider.Actor;
            OutHit.Component = InCollider.Component;
            OutHit.Location = p;
            OutHit.ImpactPoint = p;
            OutHit.Normal = n;
            OutHit.ImpactNormal = n;
            OutHit.PenetrationDepth = depth;
            OutHit.bStartPenetrating = depth > 0.0f;
            OutHit.Channel = InCollider.ObjectType;
            return true;
        }
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
        overlap = glm::max(overlap, glm::vec3(0.0f));
        if (overlap.x <= overlap.y && overlap.x <= overlap.z) {
            OutHit.Normal = (InMin.x + InMax.x < minB.x + maxB.x) ? glm::vec3(-1, 0, 0) : glm::vec3(1, 0, 0);
            OutHit.PenetrationDepth = overlap.x;
        } else if (overlap.y <= overlap.z) {
            OutHit.Normal = (InMin.y + InMax.y < minB.y + maxB.y) ? glm::vec3(0, -1, 0) : glm::vec3(0, 1, 0);
            OutHit.PenetrationDepth = overlap.y;
        } else {
            OutHit.Normal = (InMin.z + InMax.z < minB.z + maxB.z) ? glm::vec3(0, 0, -1) : glm::vec3(0, 0, 1);
            OutHit.PenetrationDepth = overlap.z;
        }
        OutHit.bStartPenetrating = OutHit.PenetrationDepth > 0.0f;
        OutHit.ImpactNormal = OutHit.Normal;
        OutHit.Channel = InCollider.ObjectType;
        return true;
    }

} // namespace Leon
