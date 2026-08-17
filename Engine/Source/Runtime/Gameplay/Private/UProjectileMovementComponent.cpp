#include "Gameplay/UProjectileMovementComponent.hpp"
#include "Gameplay/AProjectile.hpp"
#include "Gameplay/AActor.hpp"
#include "Engine/UWorld.hpp"
#include "Renderer/FRenderingMath.hpp"

#include <algorithm>
#include <cmath>

namespace Leon {

    UProjectileMovementComponent::UProjectileMovementComponent(const std::string& InName) : UActorComponent(InName) {}

    void UProjectileMovementComponent::SetVelocityInLocalSpace(const glm::vec3& InDir, float InSpeed) {
        const glm::vec3 n = glm::length(InDir) > 1e-5f ? glm::normalize(InDir) : glm::vec3(0.0f, 0.0f, 1.0f);
        Velocity = n * InSpeed;
    }

    bool UProjectileMovementComponent::SweepStep(const glm::vec3& InStart, const glm::vec3& InEnd,
                                                 FHitResult& OutHit) const {
        AActor* owner = UpdatedActor ? UpdatedActor : GetOwner();
        if (!owner || !owner->GetWorld())
            return false;
        const float radius = std::max(0.04f, ProjectileRadius);
        return owner->GetWorld()->SweepSingleByChannel(InStart, InEnd, radius, ECollisionChannel::Visibility,
                                                       IgnoreActor, OutHit);
    }

    void UProjectileMovementComponent::Tick(float DeltaSeconds) {
        AActor* owner = UpdatedActor ? UpdatedActor : GetOwner();
        if (!owner || owner->IsPendingKill() || !owner->GetWorld())
            return;

        Velocity.y -= GravityZ * ProjectileGravityScale * DeltaSeconds;
        if (MaxSpeed > 1e-3f) {
            const float speed = glm::length(Velocity);
            if (speed > MaxSpeed)
                Velocity *= MaxSpeed / speed;
        }

        const glm::vec3 start = owner->GetActorLocation();
        const glm::vec3 end = start + Velocity * DeltaSeconds;
        FHitResult hit;
        if (SweepStep(start, end, hit) && hit.bBlockingHit) {
            if (auto* proj = dynamic_cast<AProjectile*>(owner); proj && !proj->IsArmed()) {
                owner->SetActorLocation(end);
                return;
            }
            if (owner->IsPendingKill())
                return;
            owner->SetActorLocation(hit.Location);
            if (bShouldBounce && glm::length(hit.Normal) > 1e-4f) {
                Velocity = glm::reflect(Velocity, glm::normalize(hit.Normal)) * Bounciness;
            }
            if (auto* proj = dynamic_cast<AProjectile*>(owner))
                proj->NotifyHit(hit);
            return;
        }

        owner->SetActorLocation(end);
        if (bRotationFollowsVelocity && glm::length(Velocity) > 1e-4f)
            owner->SetActorRotation(Leon::EulerAligningLocalY(glm::normalize(Velocity)));
    }

} // namespace Leon
