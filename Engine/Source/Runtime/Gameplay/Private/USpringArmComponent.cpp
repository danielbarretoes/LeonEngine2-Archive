#include "Gameplay/USpringArmComponent.hpp"
#include "Gameplay/AActor.hpp"
#include "Gameplay/FGameplayDebugger.hpp"
#include "Engine/UWorld.hpp"
#include "Physics/FHitResult.hpp"
#include "Renderer/FDebugRenderer.hpp"

#include <algorithm>
#include <cmath>

namespace Leon {

    USpringArmComponent::USpringArmComponent(const std::string& InName) : UActorComponent(InName) {
        CurrentArmLength = TargetArmLength;
    }

    void USpringArmComponent::UpdateDesiredArmLocation(const glm::vec3& InOrigin, const glm::vec3& InForward,
                                                       const glm::vec3& InRight, const glm::vec3& InUp) {
        glm::vec3 origin = InOrigin + TargetOffset;
        DesiredLocation = origin - InForward * TargetArmLength + InRight * SocketOffset.x + InUp * SocketOffset.y +
                          glm::vec3(0.0f, 0.0f, 1.0f) * SocketOffset.z;
        CurrentArmLength = TargetArmLength;
        bHasHit = false;

        if (bDoCollisionTest && Owner && Owner->GetWorld()) {
            FHitResult hit;
            if (Owner->GetWorld()->SweepSingleByChannel(origin, DesiredLocation, ProbeSize, ProbeChannel, Owner, hit) &&
                hit.bBlockingHit) {
                bHasHit = true;
                LastHitLocation = hit.ImpactPoint;
                LastHitNormal = hit.ImpactNormal;
                const float dist = std::max(hit.Distance - ProbeSize, 0.05f);
                CurrentArmLength = dist;
                glm::vec3 dir = DesiredLocation - origin;
                float len = glm::length(dir);
                if (len > 1e-5f)
                    TargetLocation = origin + (dir / len) * CurrentArmLength;
                else
                    TargetLocation = origin;
            } else {
                TargetLocation = DesiredLocation;
            }
        } else {
            TargetLocation = DesiredLocation;
        }

        if (FGameplayDebugger::ShowCharacter() && FGameplayDebugger::IsEnabled()) {
            FDebugRenderer::DrawDebugLine(origin, DesiredLocation, glm::vec4(0.2f, 0.8f, 1.0f, 1.0f));
            FDebugRenderer::DrawDebugSphere(DesiredLocation, ProbeSize, glm::vec4(0.2f, 0.8f, 1.0f, 0.5f));
            FDebugRenderer::DrawDebugSphere(TargetLocation, ProbeSize, glm::vec4(0.2f, 1.0f, 0.4f, 1.0f));
            if (bHasHit)
                FDebugRenderer::DrawDebugPoint(LastHitLocation, 0.08f, glm::vec4(1.0f, 0.3f, 0.1f, 1.0f));
        }
    }

    void USpringArmComponent::Tick(float DeltaSeconds) {
        (void)DeltaSeconds;
    }

} // namespace Leon
