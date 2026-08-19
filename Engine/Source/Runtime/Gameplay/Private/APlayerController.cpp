#include "Gameplay/APlayerController.hpp"
#include "Gameplay/APlayerCameraManager.hpp"
#include "Gameplay/APawn.hpp"
#include "Gameplay/ACharacter.hpp"
#include "Engine/UWorld.hpp"
#include "Engine/Components.hpp"

#include <algorithm>

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

    void APlayerController::EnterSpectatorMode(AActor* InInitialTarget) {
        bSpectating = true;
        if (InInitialTarget)
            SetViewTargetWithBlend(InInitialTarget, 0.35f);
    }

    void APlayerController::LeaveSpectatorMode() {
        bSpectating = false;
        if (Pawn)
            SetViewTarget(Pawn);
    }

    void APlayerController::CollectSpectatorTargets(std::vector<APawn*>& OutTargets) const {
        OutTargets.clear();
        if (!World)
            return;
        for (const auto& actor : World->GetAllActors()) {
            auto* pawn = dynamic_cast<APawn*>(actor.get());
            if (pawn && pawn != Pawn && !pawn->IsPendingKill())
                OutTargets.push_back(pawn);
        }
    }

    void APlayerController::CycleSpectatorTarget(int InDirection) {
        if (!bSpectating || InDirection == 0)
            return;

        std::vector<APawn*> targets;
        CollectSpectatorTargets(targets);
        if (targets.empty())
            return;

        AActor* current = GetViewTarget();
        int idx = -1;
        for (size_t i = 0; i < targets.size(); ++i) {
            if (targets[i] == current) {
                idx = static_cast<int>(i);
                break;
            }
        }
        if (idx < 0)
            idx = 0;
        else {
            idx = (idx + InDirection) % static_cast<int>(targets.size());
            if (idx < 0)
                idx += static_cast<int>(targets.size());
        }
        SetViewTargetWithBlend(targets[static_cast<size_t>(idx)], 0.25f);
    }

    void APlayerController::UpdateCameraManager(float DeltaSeconds) {
        if (PlayerCameraManager) {
            PlayerCameraManager->UpdateCamera(DeltaSeconds);
        }
    }

    void APlayerController::GetPlayerViewPoint(FPerspectiveCamera& OutCamera) const {
        if (PlayerCameraManager)
            OutCamera = PlayerCameraManager->GetCamera();

        if (bSpectating) {
            if (auto* targetChar = dynamic_cast<ACharacter*>(GetViewTarget())) {
                glm::vec3 loc, fwd;
                targetChar->GetViewPoint(loc, fwd);
                (void)fwd;
                OutCamera.SetPosition(loc);
                OutCamera.SetRotation(targetChar->GetControlPitch(), targetChar->GetControlYaw());
                return;
            }
        }

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
