#include "Gameplay/APickup.hpp"
#include "Gameplay/APawn.hpp"
#include "Engine/UWorld.hpp"
#include "Engine/ENetTypes.hpp"

#include <cmath>

namespace Leon {

    APickup::APickup(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AActor(InHandle, InWorld, InName) {
        SetClass("APickup");
    }

    void APickup::BeginPlay() {
        AActor::BeginPlay();
        HomeLocation = GetActorLocation();

        OverlapSphere = AddActorComponent<USphereComponent>("PickupSphere");
        OverlapSphere->SetSphereRadius(PickupRadius);
        OverlapSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        OverlapSphere->SetCollisionObjectType(ECollisionChannel::WorldDynamic);
        OverlapSphere->SetCollisionResponseToAllChannels(ECollisionResponse::Overlap);
        OverlapSphere->SetGenerateOverlapEvents(true);
        OverlapSphere->OnComponentBeginOverlap.push_back(
            [this](UPrimitiveComponent*, AActor* Other, UPrimitiveComponent*, const FHitResult&) {
                if (auto* pawn = dynamic_cast<APawn*>(Other))
                    OnOverlapPawn(pawn);
            });

        BuildVisual();
        SetPickupActive(true);
    }

    void APickup::SetPickupRadius(float InRadius) {
        PickupRadius = std::max(0.05f, InRadius);
        if (OverlapSphere)
            OverlapSphere->SetSphereRadius(PickupRadius);
    }

    void APickup::SetAnchorLocation(const glm::vec3& InLocation) {
        HomeLocation = InLocation;
        SetActorLocation(InLocation);
    }

    void APickup::SetPickupActive(bool bInActive) {
        bActive = bInActive;
        if (OverlapSphere)
            OverlapSphere->SetCollisionEnabled(bInActive ? ECollisionEnabled::QueryOnly
                                                         : ECollisionEnabled::NoCollision);
        if (bInActive)
            SetActorLocation(HomeLocation);
    }

    bool APickup::CanBePickedUp(APawn* InPawn) const {
        return InPawn && !InPawn->IsPendingKill() && bActive;
    }

    void APickup::OnOverlapPawn(APawn* InPawn) {
        if (!World || World->GetNetMode() == ENetMode::Client)
            return;
        if (!CanBePickedUp(InPawn))
            return;
        if (!GiveTo(InPawn))
            return;
        SetPickupActive(false);
        RespawnRemaining = RespawnDelay;
    }

    void APickup::TryCollectOverlappingPawns() {
        if (!World || !bActive || World->GetNetMode() == ENetMode::Client)
            return;
        for (const auto& actorRef : World->GetAllActors()) {
            auto* pawn = dynamic_cast<APawn*>(actorRef.get());
            if (!CanBePickedUp(pawn))
                continue;
            const float dist = glm::length(pawn->GetActorLocation() - GetActorLocation());
            if (dist > PickupRadius)
                continue;
            if (!GiveTo(pawn))
                continue;
            SetPickupActive(false);
            RespawnRemaining = RespawnDelay;
            break;
        }
    }

    void APickup::Tick(float DeltaSeconds) {
        AActor::Tick(DeltaSeconds);
        if (!World)
            return;

        if (!bActive) {
            RespawnRemaining -= DeltaSeconds;
            if (RespawnRemaining <= 0.0f)
                SetPickupActive(true);
            return;
        }

        if (bBobEnabled) {
            BobPhase += DeltaSeconds * BobSpeed;
            glm::vec3 loc = HomeLocation;
            loc.y += std::sin(BobPhase) * BobAmplitude;
            SetActorLocation(loc);
        }

        SpinYaw += DeltaSeconds * SpinDegreesPerSecond;
        if (SpinYaw >= 360.0f || SpinYaw <= -360.0f)
            SpinYaw = std::fmod(SpinYaw, 360.0f);
        SetActorRotation({SpinTiltDegrees, SpinYaw, 0.0f});

        // Radius fallback when overlaps are not yet generating (e.g. no physics pair).
        TryCollectOverlappingPawns();
    }

} // namespace Leon
