#include "Gameplay/ANavMeshBoundsVolume.hpp"

namespace Leon {

    ANavMeshBoundsVolume::ANavMeshBoundsVolume(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AActor(InHandle, InWorld, InName) {
        SetClass("ANavMeshBoundsVolume");
        SetCanEverTick(false);
    }

    void ANavMeshBoundsVolume::GetBounds(glm::vec3& OutMin, glm::vec3& OutMax) const {
        const glm::vec3 c = GetActorLocation();
        const glm::vec3 e = GetActorScale() * 0.5f;
        OutMin = c - e;
        OutMax = c + e;
    }

} // namespace Leon
