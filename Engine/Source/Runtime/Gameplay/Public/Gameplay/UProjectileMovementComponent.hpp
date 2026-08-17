#pragma once

#include "Gameplay/UActorComponent.hpp"
#include "Physics/FHitResult.hpp"

#include <glm/glm.hpp>

namespace Leon {

    class AActor;

    /**
     * Unreal-aligned projectile integrator: velocity + gravity + sphere sweep.
     * Hit notification is dispatched to AProjectile::NotifyHit.
     */
    class UProjectileMovementComponent : public UActorComponent {
    public:
        UProjectileMovementComponent(const std::string& InName = "ProjectileMovement");

        void Tick(float DeltaSeconds) override;

        void SetUpdatedComponent(AActor* InUpdated) { UpdatedActor = InUpdated; }
        AActor* GetUpdatedComponent() const { return UpdatedActor; }

        void SetVelocity(const glm::vec3& InVelocity) { Velocity = InVelocity; }
        const glm::vec3& GetVelocity() const { return Velocity; }
        void SetVelocityInLocalSpace(const glm::vec3& InDir, float InSpeed);

        float InitialSpeed = 20.0f;
        float MaxSpeed = 0.0f;
        float ProjectileGravityScale = 0.0f;
        float GravityZ = 22.0f;
        float ProjectileRadius = 0.18f;
        bool bRotationFollowsVelocity = true;
        bool bShouldBounce = false;
        float Bounciness = 0.35f;
        AActor* IgnoreActor = nullptr;

    private:
        bool SweepStep(const glm::vec3& InStart, const glm::vec3& InEnd, FHitResult& OutHit) const;

        glm::vec3 Velocity{0.0f};
        AActor* UpdatedActor = nullptr;
    };

} // namespace Leon
