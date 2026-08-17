#pragma once

#include "Gameplay/AActor.hpp"
#include "Gameplay/UPrimitiveComponent.hpp"
#include "Gameplay/UProjectileMovementComponent.hpp"
#include "Physics/FHitResult.hpp"

namespace Leon {

    class APawn;

    /**
     * Unreal-aligned projectile actor: CollisionComponent + ProjectileMovement.
     * Subclasses override NotifyHit / OnLifeSpanExpired for damage, VFX, and destroy.
     */
    class AProjectile : public AActor {
    public:
        AProjectile() = default;
        AProjectile(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "Projectile");

        void BeginPlay() override;
        void Tick(float DeltaSeconds) override;

        TRef<USphereComponent> GetCollisionComponent() const { return CollisionComponent; }
        TRef<UProjectileMovementComponent> GetProjectileMovement() const { return ProjectileMovement; }

        void SetInstigatorPawn(APawn* InPawn);
        APawn* GetInstigatorPawn() const { return InstigatorPawn; }

        void SetInitialLifeSpan(float InSeconds) { InitialLifeSpan = InSeconds; }
        float GetInitialLifeSpan() const { return InitialLifeSpan; }

        /** Launch along world direction using ProjectileMovement::InitialSpeed. */
        void InitVelocity(const glm::vec3& InDirection);

        virtual void NotifyHit(const FHitResult& InHit);
        virtual void OnLifeSpanExpired();

        bool HasExploded() const { return bExploded; }
        bool IsArmed() const { return bArmed; }

    protected:
        void MarkExploded() { bExploded = true; }

        TRef<USphereComponent> CollisionComponent;
        TRef<UProjectileMovementComponent> ProjectileMovement;
        APawn* InstigatorPawn = nullptr;
        float InitialLifeSpan = 4.0f;
        float LifeSpanRemaining = 4.0f;
        float ArmDelay = 0.08f;
        bool bArmed = false;
        bool bExploded = false;
        bool bDestroyOnHit = true;
    };

} // namespace Leon
