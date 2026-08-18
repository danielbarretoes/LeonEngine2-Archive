#pragma once

#include "Engine/ECollisionChannel.hpp"
#include "Physics/ECollisionTypes.hpp"

#include <glm/glm.hpp>
#include <string>

namespace Leon {

    class AActor;
    class UPrimitiveComponent;

    /**
     * Generic hit payload for traces, sweeps, and overlaps.
     */
    struct FHitResult {
        bool bBlockingHit = false;
        bool bStartPenetrating = false;
        AActor* Actor = nullptr;
        UPrimitiveComponent* Component = nullptr;
        glm::vec3 Location{0.0f};
        glm::vec3 ImpactPoint{0.0f};
        glm::vec3 Normal{0.0f, 1.0f, 0.0f};
        glm::vec3 ImpactNormal{0.0f, 1.0f, 0.0f};
        float Distance = 0.0f;
        float Time = 0.0f;
        /** Positive when the query shape starts overlapping the hit (AABB SAT depth). */
        float PenetrationDepth = 0.0f;
        ECollisionChannel Channel = ECollisionChannel::Visibility;
        std::string BoneName;
        std::string PhysMaterial;
    };

} // namespace Leon
