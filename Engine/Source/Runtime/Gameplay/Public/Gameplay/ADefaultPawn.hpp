#pragma once

#include "Core/FInput.hpp"
#include "Gameplay/APawn.hpp"

namespace Leon {

    /**
     * @brief 6-DOF fly / spectator pawn (editor-style camera). Not a ground character —
     * prefer ADefaultPawn for spectator fly; use ACharacter for XZ floor movement.
     * Set DefaultPawnClass in DefaultGame.ini.
     */
    class ADefaultPawn : public APawn {
    public:
        ADefaultPawn() = default;
        ADefaultPawn(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "DefaultPawn");
        ~ADefaultPawn() override = default;

        void PostInitializeComponents() override;
        void Tick(float DeltaSeconds) override;
        void SetupPlayerInputComponent(float DeltaSeconds) override;

        float GetMoveSpeed() const { return MoveSpeed; }
        void SetMoveSpeed(float InSpeed) { MoveSpeed = InSpeed; }

        float GetLookSensitivity() const { return LookSensitivity; }
        void SetLookSensitivity(float InSens) { LookSensitivity = InSens; }

    private:
        float MoveSpeed = 8.0f;
        float SprintMultiplier = 2.5f;
        float LookSensitivity = 0.12f;

        float Yaw = -90.0f;
        float Pitch = 0.0f;
        glm::vec2 LastMousePos{0.0f, 0.0f};
        bool bFirstMouse = true;
    };

} // namespace Leon
