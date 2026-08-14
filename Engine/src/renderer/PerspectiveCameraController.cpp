#include "renderer/PerspectiveCameraController.hpp"
#include "core/Input.hpp"
#include <cmath>

namespace Leon {

    FPerspectiveCameraController::FPerspectiveCameraController(float InFOV, float InAspectRatio, float InNearClip,
                                                               float InFarClip)
        : m_Camera(InFOV, InAspectRatio, InNearClip, InFarClip) {}

    void FPerspectiveCameraController::OnUpdate(FTimestep InTs) {
        float speed = m_TranslationSpeed * InTs.GetSeconds();

        // ----------------------------------------------------
        // 1. Keyboard & Mouse Controls
        // ----------------------------------------------------
        // Speed boost with LeftShift
        if (FInput::IsKeyPressed(Key::LeftShift)) {
            speed *= 2.5f;
        }

        glm::vec3 position = m_Camera.GetPosition();
        glm::vec3 forward = m_Camera.GetForwardDirection();
        glm::vec3 right = m_Camera.GetRightDirection();
        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

        // WASD & Space/LeftControl Translation
        if (FInput::IsKeyPressed(Key::W))
            position += forward * speed;
        if (FInput::IsKeyPressed(Key::S))
            position -= forward * speed;
        if (FInput::IsKeyPressed(Key::D))
            position += right * speed;
        if (FInput::IsKeyPressed(Key::A))
            position -= right * speed;
        if (FInput::IsKeyPressed(Key::Space) || FInput::IsKeyPressed(Key::E))
            position += up * speed;
        if (FInput::IsKeyPressed(Key::LeftControl) || FInput::IsKeyPressed(Key::Q))
            position -= up * speed;

        // Mouse Look on Right Click Drag or Left Click Drag
        if (FInput::IsMouseButtonPressed(Mouse::ButtonRight) || FInput::IsMouseButtonPressed(Mouse::ButtonLeft)) {
            auto [mouseX, mouseY] = FInput::GetMousePosition();

            if (bFirstMouse) {
                m_LastMousePosition = {mouseX, mouseY};
                bFirstMouse = false;
            }

            float xOffset = (mouseX - m_LastMousePosition.x) * m_RotationSpeed;
            float yOffset = (m_LastMousePosition.y - mouseY) * m_RotationSpeed;

            m_LastMousePosition = {mouseX, mouseY};

            float newYaw = m_Camera.GetYaw() + xOffset;
            float newPitch = m_Camera.GetPitch() + yOffset;

            m_Camera.SetRotation(newPitch, newYaw);
        } else {
            bFirstMouse = true;
        }

        // ----------------------------------------------------
        // 2. Xbox / Gamepad Controller Controls
        // ----------------------------------------------------
        if (FInput::IsGamepadConnected(0)) {
            // Speed boost with L3 or RB
            if (FInput::IsGamepadButtonPressed(GamepadButton::LeftThumb, 0) ||
                FInput::IsGamepadButtonPressed(GamepadButton::RightBumper, 0)) {
                speed *= 2.5f;
            }

            // Left Stick: 3D Movement (Forward/Back & Strafe Left/Right)
            auto [leftX, leftY] = FInput::GetGamepadLeftStick(0, m_GamepadDeadzone);
            if (std::abs(leftY) > 0.0f)
                position -= forward * (leftY * speed); // Left Stick Up is negative Y (-1.0f) in GLFW
            if (std::abs(leftX) > 0.0f)
                position += right * (leftX * speed);

            // Triggers / Buttons: Elevation (Up / Down)
            // RT or A: Fly Up
            float rt = FInput::GetGamepadAxis(GamepadAxis::RightTrigger, 0, 0.0f);
            if (rt > -0.9f || FInput::IsGamepadButtonPressed(GamepadButton::A, 0)) {
                float intensity = (rt > -0.9f) ? ((rt + 1.0f) * 0.5f) : 1.0f;
                position += up * (speed * intensity);
            }

            // LT or B: Fly Down
            float lt = FInput::GetGamepadAxis(GamepadAxis::LeftTrigger, 0, 0.0f);
            if (lt > -0.9f || FInput::IsGamepadButtonPressed(GamepadButton::B, 0)) {
                float intensity = (lt > -0.9f) ? ((lt + 1.0f) * 0.5f) : 1.0f;
                position -= up * (speed * intensity);
            }

            // Right Stick: Camera Look (Yaw & Pitch)
            auto [rightX, rightY] = FInput::GetGamepadRightStick(0, m_GamepadDeadzone);
            if (std::abs(rightX) > 0.0f || std::abs(rightY) > 0.0f) {
                float yawOffset = rightX * m_GamepadRotationSpeed * InTs.GetSeconds();
                float pitchOffset = -rightY * m_GamepadRotationSpeed * InTs.GetSeconds();

                m_Camera.SetRotation(m_Camera.GetPitch() + pitchOffset, m_Camera.GetYaw() + yawOffset);
            }
        }

        m_Camera.SetPosition(position);
    }

    void FPerspectiveCameraController::OnEvent(FEvent& InEvent) {
        FEventDispatcher dispatcher(InEvent);
        dispatcher.Dispatch<FMouseScrolledEvent>(LE_BIND_EVENT_FN(FPerspectiveCameraController::OnMouseScrolled));
        dispatcher.Dispatch<FWindowResizeEvent>(LE_BIND_EVENT_FN(FPerspectiveCameraController::OnWindowResized));
    }

    bool FPerspectiveCameraController::OnMouseScrolled(FMouseScrolledEvent& InEvent) {
        float fov = m_Camera.GetFOV();
        fov -= InEvent.GetYOffset() * 2.0f;
        m_Camera.SetFOV(fov);
        return false;
    }

    bool FPerspectiveCameraController::OnWindowResized(FWindowResizeEvent& InEvent) {
        m_Camera.SetViewportSize(InEvent.GetWidth(), InEvent.GetHeight());
        return false;
    }

} // namespace Leon
