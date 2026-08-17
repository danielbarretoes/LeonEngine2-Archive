#include "Gameplay/APlayerCameraManager.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Gameplay/APawn.hpp"
#include "Engine/UWorld.hpp"

namespace Leon {

    APlayerCameraManager::APlayerCameraManager(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AActor(InHandle, InWorld, InName) {
        SetClass("APlayerCameraManager");
        Camera.SetPosition({0.0f, 3.5f, 10.5f});
        Camera.SetRotation(-10.0f, -90.0f);
    }

    void APlayerCameraManager::InitializeFor(APlayerController* InPC) {
        PlayerController = InPC;
    }

    void APlayerCameraManager::SetViewTarget(AActor* InNewTarget) {
        ViewTarget = InNewTarget;
    }

    void APlayerCameraManager::SetAspectRatio(float InAspect) {
        Camera.SetProjection(Camera.GetFOV(), InAspect, Camera.GetNearClip(), Camera.GetFarClip());
    }

    void APlayerCameraManager::UpdateCamera(float DeltaSeconds) {
        // Priority 1: Explicit ViewTarget set on CameraManager
        if (ViewTarget && ViewTarget->HasComponent<FCameraComponent>()) {
            const auto& camComp = ViewTarget->GetComponent<FCameraComponent>();
            Camera = camComp.Camera;
            return;
        }

        // Priority 2: Possessed Pawn of the PlayerController
        if (PlayerController) {
            APawn* pawn = PlayerController->GetPawn();
            if (pawn && pawn->HasComponent<FCameraComponent>()) {
                const auto& camComp = pawn->GetComponent<FCameraComponent>();
                Camera = camComp.Camera;
                return;
            }
        }

        // Priority 3: Search World for an Actor with a primary FCameraComponent
        if (World) {
            auto view = World->GetRegistry().view<FCameraComponent, FTransformComponent>();
            for (auto entity : view) {
                const auto& camComp = view.get<FCameraComponent>(entity);
                if (camComp.bPrimary) {
                    Camera = camComp.Camera;
                    return;
                }
            }
        }

        // Priority 4: Maintain fallback camera parameters
    }

} // namespace Leon
