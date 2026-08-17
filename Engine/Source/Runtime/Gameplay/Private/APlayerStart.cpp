#include "Gameplay/APlayerStart.hpp"
#include "Gameplay/FGameplayDebugger.hpp"
#include "Renderer/FDebugRenderer.hpp"

#include <cmath>

namespace Leon {

    APlayerStart::APlayerStart(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AActor(InHandle, InWorld, InName) {
        SetClass("APlayerStart");
        SetCanEverTick(true);
    }

    void APlayerStart::Tick(float DeltaSeconds) {
        (void)DeltaSeconds;
        if (!FGameplayDebugger::ShowPhysics())
            return;
        const glm::vec3 origin = GetActorLocation();
        FDebugRenderer::DrawDebugCapsule(origin, 0.4f, 0.95f, glm::vec4(0.95f, 0.85f, 0.15f, 1.0f));
        const glm::vec3 rot = GetActorRotation();
        const float yawRad = glm::radians(rot.y);
        const glm::vec3 fwd(std::cos(yawRad), 0.0f, std::sin(yawRad));
        FDebugRenderer::DrawDebugArrow(origin, origin + fwd * 1.6f, glm::vec4(1.0f, 0.9f, 0.2f, 1.0f), 0.25f);
    }

} // namespace Leon
