#pragma once

#include "Physics/ECollisionTypes.hpp"
#include "Physics/FHitResult.hpp"

#include <glm/glm.hpp>
#include <vector>

namespace Leon {

    class AActor;
    class UPrimitiveComponent;
    class UWorld;

    struct FColliderDesc {
        AActor* Actor = nullptr;
        UPrimitiveComponent* Component = nullptr;
        EPhysicsShapeType Shape = EPhysicsShapeType::Box;
        glm::vec3 Center{0.0f};
        glm::vec3 BoxHalfExtent{0.5f};
        float SphereRadius = 0.5f;
        float CapsuleRadius = 0.4f;
        float CapsuleHalfHeight = 0.9f;
        ECollisionChannel ObjectType = ECollisionChannel::WorldStatic;
        FCollisionResponseContainer Responses;
        ECollisionEnabled CollisionEnabled = ECollisionEnabled::QueryAndPhysics;
    };

    bool GatherActorColliders(AActor& InActor, std::vector<FColliderDesc>& OutColliders);
    void GatherWorldColliders(UWorld& InWorld, AActor* InIgnore, std::vector<FColliderDesc>& OutColliders);

    /** Wireframe all gathered colliders (Shift+F1 physics). Color encodes ObjectType. */
    void DrawDebugWorldColliders(UWorld& InWorld);

    bool ColliderRespondsToChannel(const FColliderDesc& InCollider, ECollisionChannel InQuery);
    void ColliderWorldAABB(const FColliderDesc& InCollider, glm::vec3& OutMin, glm::vec3& OutMax);

    bool RaycastCollider(const FColliderDesc& InCollider, const glm::vec3& InStart, const glm::vec3& InDir, float InLen,
                         FHitResult& OutHit);
    bool SweepSphereCollider(const FColliderDesc& InCollider, const glm::vec3& InStart, const glm::vec3& InEnd,
                             float InRadius, FHitResult& OutHit);
    bool OverlapAABBCollider(const FColliderDesc& InCollider, const glm::vec3& InMin, const glm::vec3& InMax,
                             FHitResult& OutHit);

} // namespace Leon
