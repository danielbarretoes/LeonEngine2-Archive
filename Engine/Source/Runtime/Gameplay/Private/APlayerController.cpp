#include "Gameplay/APlayerController.hpp"
#include "Gameplay/APlayerCameraManager.hpp"
#include "Gameplay/APawn.hpp"
#include "Gameplay/ACharacter.hpp"
#include "Engine/UWorld.hpp"
#include "Engine/Components.hpp"

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
        SetViewTargetWithBlend(InNewTarget, 0.0f);
    }

    void APlayerController::SetViewTargetWithBlend(AActor* InNewTarget, float InBlendTime) {
        if (PlayerCameraManager)
            PlayerCameraManager->SetViewTarget(InNewTarget, InBlendTime);
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
        if (PlayerCameraManager)
            OutCamera = PlayerCameraManager->GetCamera();
        // Live character view (incl. spring arm) so render / HUD / hitscan share one ray.
        if (auto* character = dynamic_cast<ACharacter*>(Pawn)) {
            glm::vec3 loc, fwd;
            character->GetViewPoint(loc, fwd);
            (void)fwd;
            OutCamera.SetPosition(loc);
            OutCamera.SetRotation(character->GetControlPitch(), character->GetControlYaw());
            return;
        }
        if (Pawn && Pawn->HasComponent<FCameraComponent>()) {
            OutCamera = Pawn->GetComponent<FCameraComponent>().Camera;
            OutCamera.SetPosition(Pawn->GetActorLocation());
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
