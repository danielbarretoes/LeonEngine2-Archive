#include "Gameplay/ATriggerVolume.hpp"
#include "Engine/Components.hpp"

namespace Leon {

    ATriggerVolume::ATriggerVolume(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AActor(InHandle, InWorld, InName) {
        SetClass("ATriggerVolume");
    }

    void ATriggerVolume::PostInitializeComponents() {
        if (!Box)
            Box = AddActorComponent<UBoxComponent>("Collision");
        Box->SetCollisionObjectType(ECollisionChannel::WorldDynamic);
        Box->SetBoxExtent(GetActorScale() * 0.5f);
        ApplyCollisionState();

        if (!HasComponent<FBoxCollisionComponent>()) {
            AddComponent<FBoxCollisionComponent>(glm::vec3(-0.5f), glm::vec3(0.5f), false,
                                                 ECollisionChannel::WorldDynamic);
        } else {
            GetComponent<FBoxCollisionComponent>().bBlockMovement = false;
        }
        SetCanEverTick(false);
    }

    void ATriggerVolume::SetEnabled(bool bInEnabled) {
        bEnabled = bInEnabled;
        ApplyCollisionState();
        if (HasComponent<FBoxCollisionComponent>())
            GetComponent<FBoxCollisionComponent>().bBlockMovement = false;
    }

    void ATriggerVolume::ApplyCollisionState() {
        if (!Box)
            return;
        if (bEnabled) {
            Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            Box->SetCollisionResponseToAllChannels(ECollisionResponse::Overlap);
            Box->SetGenerateOverlapEvents(true);
        } else {
            Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Box->SetGenerateOverlapEvents(false);
        }
    }

} // namespace Leon
