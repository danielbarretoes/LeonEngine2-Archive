#include "gameplay/APlayerController.hpp"
#include "gameplay/APlayerCameraManager.hpp"
#include "gameplay/APawn.hpp"
#include "world/UWorld.hpp"

namespace Leon {

    APlayerController::APlayerController(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AActor(InHandle, InWorld, InName) {}

    void APlayerController::PostInitializeComponents() {
        if (!m_PlayerCameraManager && m_World) {
            m_PlayerCameraManager = m_World->SpawnActor<APlayerCameraManager>("PlayerCameraManager");
            if (m_PlayerCameraManager) {
                m_PlayerCameraManager->InitializeFor(this);
            }
        }
    }

    void APlayerController::Tick(float DeltaSeconds) {
        UpdateCameraManager(DeltaSeconds);
    }

    void APlayerController::Possess(APawn* InPawn) {
        if (m_Pawn == InPawn) return;

        if (m_Pawn) {
            UnPossess();
        }

        m_Pawn = InPawn;
        if (m_Pawn) {
            m_Pawn->PossessedBy(this);
        }
    }

    void APlayerController::UnPossess() {
        if (m_Pawn) {
            m_Pawn->UnPossessed();
            m_Pawn = nullptr;
        }
    }

    void APlayerController::SetViewTarget(AActor* InNewTarget) {
        if (m_PlayerCameraManager) {
            m_PlayerCameraManager->SetViewTarget(InNewTarget);
        }
    }

    AActor* APlayerController::GetViewTarget() const {
        return m_PlayerCameraManager ? m_PlayerCameraManager->GetViewTarget() : nullptr;
    }

    void APlayerController::UpdateCameraManager(float DeltaSeconds) {
        if (m_PlayerCameraManager) {
            m_PlayerCameraManager->UpdateCamera(DeltaSeconds);
        }
    }

    void APlayerController::GetPlayerViewPoint(FPerspectiveCamera& OutCamera) const {
        if (m_PlayerCameraManager) {
            OutCamera = m_PlayerCameraManager->GetCamera();
        } else if (m_Pawn && m_Pawn->HasComponent<UCameraComponent>()) {
            OutCamera = m_Pawn->GetComponent<UCameraComponent>().Camera;
        }
    }

} // namespace Leon
