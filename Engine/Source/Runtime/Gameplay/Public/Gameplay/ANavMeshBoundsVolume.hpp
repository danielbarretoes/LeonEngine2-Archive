#pragma once

#include "Gameplay/AActor.hpp"

namespace Leon {

    /**
     * Volume that defines where the navigation grid is built.
     * Actor location is the centre; actor scale is the full XYZ size in metres.
     */
    class ANavMeshBoundsVolume : public AActor {
    public:
        ANavMeshBoundsVolume() = default;
        ANavMeshBoundsVolume(entt::entity InHandle, UWorld* InWorld,
                             const std::string& InName = "NavMeshBoundsVolume");

        void GetBounds(glm::vec3& OutMin, glm::vec3& OutMax) const;
    };

} // namespace Leon
