#include "Gameplay/APhysicsVolume.hpp"

namespace Leon {

    APhysicsVolume::APhysicsVolume(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AActor(InHandle, InWorld, InName) {
        SetClass("APhysicsVolume");
    }

    void APhysicsVolume::PostInitializeComponents() {
        if (!Box)
            Box = AddActorComponent<UBoxComponent>("Volume");
        Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Box->SetCollisionResponseToAllChannels(ECollisionResponse::Overlap);
        Box->SetGenerateOverlapEvents(true);
        Box->SetBoxExtent(GetActorScale() * 0.5f);
    }

    bool APhysicsVolume::EncompassesPoint(const glm::vec3& InPoint) const {
        if (!Box)
            return false;
        glm::vec3 c = Box->GetComponentLocation();
        glm::vec3 e = Box->GetBoxExtent();
        return InPoint.x >= c.x - e.x && InPoint.x <= c.x + e.x && InPoint.y >= c.y - e.y && InPoint.y <= c.y + e.y &&
               InPoint.z >= c.z - e.z && InPoint.z <= c.z + e.z;
    }

} // namespace Leon
