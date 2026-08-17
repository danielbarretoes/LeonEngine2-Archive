#include "Gameplay/AController.hpp"
#include "Gameplay/APawn.hpp"
#include "Gameplay/ACharacter.hpp"
#include "Gameplay/UCharacterMovementComponent.hpp"
#include "Core/FLog.hpp"

namespace Leon {

    AController::AController(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AActor(InHandle, InWorld, InName) {
        SetClass("AController");
    }

    void AController::Possess(APawn* InPawn) {
        if (Pawn == InPawn)
            return;
        if (Pawn)
            UnPossess();
        Pawn = InPawn;
        if (Pawn) {
            Pawn->PossessedBy(this);
            if (auto* character = dynamic_cast<ACharacter*>(Pawn))
                character->SetControlRotation(ControlRotation);
        }
    }

    void AController::UnPossess() {
        if (Pawn) {
            Pawn->UnPossessed();
            Pawn = nullptr;
        }
        bHasMoveRequest = false;
    }

    void AController::MoveToLocation(const glm::vec3& InDest, float InAcceptanceRadius) {
        MoveDestination = InDest;
        MoveGoal = nullptr;
        MoveAcceptanceRadius = InAcceptanceRadius;
        bHasMoveRequest = true;
        auto* character = GetPawn<ACharacter>();
        if (!character || !character->GetCharacterMovement())
            return;
        glm::vec3 to = InDest - character->GetActorLocation();
        to.y = 0.0f;
        if (glm::length(to) <= InAcceptanceRadius) {
            character->GetCharacterMovement()->StopActiveMovement();
            bHasMoveRequest = false;
            return;
        }
        glm::vec3 vel = glm::normalize(to) * character->GetCharacterMovement()->GetMaxWalkSpeed();
        character->GetCharacterMovement()->RequestDirectMove(vel, true);
    }

    void AController::MoveToActor(AActor* InGoal, float InAcceptanceRadius) {
        MoveGoal = InGoal;
        if (InGoal)
            MoveToLocation(InGoal->GetActorLocation(), InAcceptanceRadius);
    }

    void AController::StopMovement() {
        bHasMoveRequest = false;
        auto* character = GetPawn<ACharacter>();
        if (character && character->GetCharacterMovement())
            character->GetCharacterMovement()->StopActiveMovement();
    }

} // namespace Leon
