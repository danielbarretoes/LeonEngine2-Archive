#include "gameplay/APlayerCameraManager.hpp"
#include "gameplay/APlayerController.hpp"
#include "gameplay/APawn.hpp"
#include "world/UWorld.hpp"

namespace Leon {

    APlayerCameraManager::APlayerCameraManager(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AActor(InHandle, InWorld, InName) {
        m_Camera.SetPosition({0.0f, 3.5f, 10.5f});
        m_Camera.SetRotation(-10.0f, -90.0f);
    }

    void APlayerCameraManager::InitializeFor(APlayerController* InPC) {
        m_PlayerController = InPC;
    }

    void APlayerCameraManager::SetViewTarget(AActor* InNewTarget) {
        m_ViewTarget = InNewTarget;
    }

    void APlayerCameraManager::SetAspectRatio(float InAspect) {
        m_Camera.SetProjection(m_Camera.GetFOV(), InAspect, m_Camera.GetNearClip(), m_Camera.GetFarClip());
    }

    void APlayerCameraManager::UpdateCamera(float DeltaSeconds) {
        // Priority 1: Explicit ViewTarget set on CameraManager
        if (m_ViewTarget && m_ViewTarget->HasComponent<UCameraComponent>()) {
            const auto& camComp = m_ViewTarget->GetComponent<UCameraComponent>();
            m_Camera = camComp.Camera;
            return;
        }

        // Priority 2: Possessed Pawn of the PlayerController
        if (m_PlayerController) {
            APawn* pawn = m_PlayerController->GetPawn();
            if (pawn && pawn->HasComponent<UCameraComponent>()) {
                const auto& camComp = pawn->GetComponent<UCameraComponent>();
                m_Camera = camComp.Camera;
                return;
            }
        }

        // Priority 3: Search World for an Actor with a primary UCameraComponent
        if (m_World) {
            auto view = m_World->GetRegistry().view<UCameraComponent, FTransformComponent>();
            for (auto entity : view) {
                const auto& camComp = view.get<UCameraComponent>(entity);
                if (camComp.bPrimary) {
                    m_Camera = camComp.Camera;
                    return;
                }
            }
        }

        // Priority 4: Maintain fallback camera parameters
    }

} // namespace Leon
