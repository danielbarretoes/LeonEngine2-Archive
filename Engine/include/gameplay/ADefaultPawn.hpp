#pragma once

#include "core/Input.hpp"
#include "gameplay/APawn.hpp"

namespace Leon {

    /**
     * @brief Unreal Engine aligned DefaultPawn providing 6-DOF spectator navigation.
     */
    class ADefaultPawn : public APawn {
    public:
        ADefaultPawn() = default;
        ADefaultPawn(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "DefaultPawn");
        ~ADefaultPawn() override = default;

        void PostInitializeComponents() override;
        void Tick(float DeltaSeconds) override;
        void SetupPlayerInputComponent(float DeltaSeconds) override;

        float GetMoveSpeed() const { return m_MoveSpeed; }
        void SetMoveSpeed(float InSpeed) { m_MoveSpeed = InSpeed; }

        float GetLookSensitivity() const { return m_LookSensitivity; }
        void SetLookSensitivity(float InSens) { m_LookSensitivity = InSens; }

    private:
        float m_MoveSpeed = 8.0f;
        float m_SprintMultiplier = 2.5f;
        float m_LookSensitivity = 0.12f;

        float m_Yaw = -90.0f;
        float m_Pitch = 0.0f;
        glm::vec2 m_LastMousePos{0.0f, 0.0f};
        bool m_bFirstMouse = true;
    };

} // namespace Leon
