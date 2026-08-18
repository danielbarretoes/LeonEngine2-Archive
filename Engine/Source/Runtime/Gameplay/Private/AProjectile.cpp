#include "Gameplay/AProjectile.hpp"
#include "Gameplay/APawn.hpp"
#include "Engine/UWorld.hpp"
#include "Renderer/FRenderingMath.hpp"

namespace Leon {

    AProjectile::AProjectile(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AActor(InHandle, InWorld, InName) {
        SetClass("AProjectile");
        SetReplicates(true);
        SetAlwaysRelevant(true);
        CollisionComponent = AddActorComponent<USphereComponent>("CollisionComponent");
        CollisionComponent->SetSphereRadius(0.18f);
        CollisionComponent->SetCollisionObjectType(ECollisionChannel::WorldDynamic);
        CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        CollisionComponent->SetGenerateOverlapEvents(false);
        SetRootComponent(CollisionComponent.get());

        ProjectileMovement = AddActorComponent<UProjectileMovementComponent>("ProjectileMovement");
        ProjectileMovement->SetUpdatedComponent(this);
    }

    void AProjectile::BeginPlay() {
        AActor::BeginPlay();
        LifeSpanRemaining = InitialLifeSpan;
        if (ProjectileMovement) {
            ProjectileMovement->SetUpdatedComponent(this);
            ProjectileMovement->IgnoreActor = InstigatorPawn;
            ProjectileMovement->ProjectileRadius =
                CollisionComponent ? CollisionComponent->GetSphereRadius() : ProjectileMovement->ProjectileRadius;
        }
    }

    void AProjectile::SetInstigatorPawn(APawn* InPawn) {
        InstigatorPawn = InPawn;
        if (ProjectileMovement)
            ProjectileMovement->IgnoreActor = InPawn;
    }

    void AProjectile::InitVelocity(const glm::vec3& InDirection) {
        if (!ProjectileMovement)
            return;
        const float speed = ProjectileMovement->InitialSpeed;
        ProjectileMovement->SetVelocityInLocalSpace(InDirection, speed);
        if (ProjectileMovement->bRotationFollowsVelocity && glm::length(InDirection) > 1e-5f)
            SetActorRotation(Leon::EulerAligningLocalY(glm::normalize(InDirection)));
    }

    void AProjectile::NotifyHit(const FHitResult& InHit) {
        if (bExploded)
            return;
        MarkExploded();
        (void)InHit;
        if (bDestroyOnHit && World)
            World->DestroyActor(this);
    }

    void AProjectile::OnLifeSpanExpired() {
        if (bExploded)
            return;
        FHitResult timeout;
        timeout.bBlockingHit = false;
        timeout.Location = GetActorLocation();
        timeout.ImpactPoint = timeout.Location;
        timeout.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
        NotifyHit(timeout);
    }

    void AProjectile::Tick(float DeltaSeconds) {
        // Simulated proxies take pose from net snapshots; skip local ballistic simulation.
        if (GetLocalRole() == ENetRole::SimulatedProxy) {
            AActor::Tick(DeltaSeconds);
            return;
        }
        AActor::Tick(DeltaSeconds);
        if (bExploded)
            return;

        ArmDelay -= DeltaSeconds;
        if (ArmDelay <= 0.0f)
            bArmed = true;
        if (ProjectileMovement)
            ProjectileMovement->IgnoreActor = InstigatorPawn;

        LifeSpanRemaining -= DeltaSeconds;
        if (LifeSpanRemaining <= 0.0f)
            OnLifeSpanExpired();
    }

} // namespace Leon
