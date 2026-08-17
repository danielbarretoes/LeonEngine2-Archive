#include "Gameplay/ABlockingVolume.hpp"

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
        SetCanEverTick(false);
    }

    void ABlockingVolume::Tick(float DeltaSeconds) {
        (void)DeltaSeconds;
    }

} // namespace Leon
