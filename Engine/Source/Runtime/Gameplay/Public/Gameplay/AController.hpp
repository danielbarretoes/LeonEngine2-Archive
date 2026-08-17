#pragma once

#include "Gameplay/AActor.hpp"

#include <glm/glm.hpp>
#include <algorithm>

namespace Leon {

    class APawn;
    class APlayerState;

    /**
     * Base controller for possessed pawns. Player and AI both derive from this.
     */
    class AController : public AActor {
    public:
        AController() = default;
        AController(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "Controller");
        ~AController() override = default;

        virtual void Possess(APawn* InPawn);
        virtual void UnPossess();

        APawn* GetPawn() const { return Pawn; }
        template <typename T> T* GetPawn() const { return dynamic_cast<T*>(Pawn); }

        APlayerState* GetPlayerState() const { return PlayerState; }
        void SetPlayerState(APlayerState* InPlayerState) { PlayerState = InPlayerState; }

        virtual void MoveToLocation(const glm::vec3& InDest, float InAcceptanceRadius = 0.6f);
        virtual void MoveToActor(AActor* InGoal, float InAcceptanceRadius = 0.6f);
        virtual void StopMovement();

        /**
         * ControlRotation: Pitch/Yaw/Roll in degrees (same as FTransformComponent).
         * TPS: Pitch drives the camera only; Yaw orients the pawn on the ground plane; Roll stays 0.
         */
        const glm::vec3& GetControlRotation() const { return ControlRotation; }
        void SetControlRotation(const glm::vec3& InRotation) {
            ControlRotation = InRotation;
            ControlRotation.x = std::clamp(ControlRotation.x, -89.0f, 89.0f);
            ControlRotation.z = 0.0f;
        }
        void AddYawInput(float InDeltaYaw) { SetControlRotation({ControlRotation.x, ControlRotation.y + InDeltaYaw, 0.0f}); }
        void AddPitchInput(float InDeltaPitch) {
            SetControlRotation({ControlRotation.x + InDeltaPitch, ControlRotation.y, 0.0f});
        }

        bool HasActiveMoveRequest() const { return bHasMoveRequest; }
        const glm::vec3& GetMoveDestination() const { return MoveDestination; }
        AActor* GetMoveGoal() const { return MoveGoal; }
        float GetMoveAcceptanceRadius() const { return MoveAcceptanceRadius; }

    protected:
        APawn* Pawn = nullptr;
        APlayerState* PlayerState = nullptr;
        glm::vec3 ControlRotation{0.0f, -90.0f, 0.0f};
        glm::vec3 MoveDestination{0.0f};
        AActor* MoveGoal = nullptr;
        float MoveAcceptanceRadius = 0.6f;
        bool bHasMoveRequest = false;
    };

} // namespace Leon
