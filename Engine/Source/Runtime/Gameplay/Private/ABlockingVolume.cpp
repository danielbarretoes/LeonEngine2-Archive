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

} // namespace Leon
