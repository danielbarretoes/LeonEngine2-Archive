#include "Gameplay/ABlockingVolume.hpp"
#include "Gameplay/FGameplayDebugger.hpp"
#include "Renderer/FDebugRenderer.hpp"

namespace Leon {

    ABlockingVolume::ABlockingVolume(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AActor(InHandle, InWorld, InName) {
        SetClass("ABlockingVolume");
    }

    void ABlockingVolume::PostInitializeComponents() {
        if (!Box)
            Box = AddActorComponent<UBoxComponent>("Collision");
        Box->SetCollisionObjectType(ECollisionChannel::WorldStatic);
        Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Box->SetCollisionResponseToAllChannels(ECollisionResponse::Block);
        Box->SetBoxExtent(GetActorScale() * 0.5f);
        SetCanEverTick(true);
    }

    void ABlockingVolume::Tick(float DeltaSeconds) {
        (void)DeltaSeconds;
        if (!FGameplayDebugger::IsEnabled() || !FGameplayDebugger::ShowPhysics() || !Box)
            return;
        FDebugRenderer::DrawDebugBox(GetActorLocation(), Box->GetBoxExtent(), glm::vec4(0.2f, 1.0f, 0.3f, 0.8f));
    }

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
