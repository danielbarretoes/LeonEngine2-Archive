#pragma once

#include "Core/FInput.hpp"
#include "Core/FInputSettings.hpp"
#include "Gameplay/APawn.hpp"

namespace Leon {

    /**
     * @brief Thin ground character: yaw look + XZ movement, fixed floor height (no physics).
     *
     * Set DefaultPawnClass=ACharacter in DefaultGame.ini to use instead of fly spectator ADefaultPawn.
     */
    class ACharacter : public APawn {
    public:
        ACharacter() = default;
        ACharacter(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "Character");
        ~ACharacter() override = default;

        void PostInitializeComponents() override;
        void Tick(float DeltaSeconds) override;
        void SetupPlayerInputComponent(float DeltaSeconds) override;

        float GetMoveSpeed() const { return MoveSpeed; }
        void SetMoveSpeed(float InSpeed) { MoveSpeed = InSpeed; }

        float GetFloorZ() const { return FloorZ; }
        void SetFloorZ(float InZ) { FloorZ = InZ; }

        float GetEyeHeight() const { return EyeHeight; }
        void SetEyeHeight(float InHeight) { EyeHeight = InHeight; }

    private:
        float MoveSpeed = 6.0f;
        float SprintMultiplier = 1.8f;
        float LookSensitivity = 0.12f;
        float FloorZ = 0.0f;
        float EyeHeight = 1.7f;

        float Yaw = -90.0f;
        float Pitch = 0.0f;
        glm::vec2 LastMousePos{0.0f, 0.0f};
        bool bFirstMouse = true;
    };

} // namespace Leon
