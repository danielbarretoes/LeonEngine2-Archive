#include "Gameplay/APlayerController.hpp"
#include "Gameplay/APlayerCameraManager.hpp"
#include "Gameplay/APawn.hpp"
#include "Engine/UWorld.hpp"

namespace Leon {

    APlayerController::APlayerController(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AController(InHandle, InWorld, InName) {
        SetClass("APlayerController");
    }

    void APlayerController::PostInitializeComponents() {
        if (!PlayerCameraManager && World) {
            PlayerCameraManager = World->SpawnActor<APlayerCameraManager>("PlayerCameraManager");
            if (PlayerCameraManager) {
                PlayerCameraManager->InitializeFor(this);
            }
        }
    }

    void APlayerController::Tick(float DeltaSeconds) {
        UpdateCameraManager(DeltaSeconds);
    }

    void APlayerController::SetViewTarget(AActor* InNewTarget) {
        if (PlayerCameraManager) {
            PlayerCameraManager->SetViewTarget(InNewTarget);
        }
    }

    AActor* APlayerController::GetViewTarget() const {
        return PlayerCameraManager ? PlayerCameraManager->GetViewTarget() : nullptr;
    }

    void APlayerController::UpdateCameraManager(float DeltaSeconds) {
        if (PlayerCameraManager) {
            PlayerCameraManager->UpdateCamera(DeltaSeconds);
        }
    }

    void APlayerController::GetPlayerViewPoint(FPerspectiveCamera& OutCamera) const {
        if (PlayerCameraManager) {
            OutCamera = PlayerCameraManager->GetCamera();
        } else if (Pawn && Pawn->HasComponent<UCameraComponent>()) {
            OutCamera = Pawn->GetComponent<UCameraComponent>().Camera;
        }
    }

    void APlayerController::SetInputModeGameOnly() {
        InputMode = EInputMode::GameOnly;
        bShowMouseCursor = false;
    }

    void APlayerController::SetInputModeUIOnly() {
        InputMode = EInputMode::UIOnly;
        bShowMouseCursor = true;
    }

    void APlayerController::SetInputModeGameAndUI() {
        InputMode = EInputMode::GameAndUI;
        bShowMouseCursor = true;
    }

    void APlayerController::SetShowMouseCursor(bool bShow) {
        bShowMouseCursor = bShow;
    }

} // namespace Leon
