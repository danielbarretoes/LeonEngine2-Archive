#pragma once

#include "engine/core/Base.hpp"
#include "engine/core/Timestep.hpp"
#include "engine/core/events/ApplicationEvent.hpp"
#include "engine/core/events/MouseEvent.hpp"
#include "engine/renderer/PerspectiveCamera.hpp"

namespace Leon {

    class FPerspectiveCameraController {
    public:
        FPerspectiveCameraController(float InFOV = 45.0f, float InAspectRatio = 16.0f / 9.0f, float InNearClip = 0.1f,
                                     float InFarClip = 1000.0f);

        void OnUpdate(FTimestep InTs);
        void OnEvent(FEvent& InEvent);

        FPerspectiveCamera& GetCamera() { return m_Camera; }
        const FPerspectiveCamera& GetCamera() const { return m_Camera; }

        float GetTranslationSpeed() const { return m_TranslationSpeed; }
        void SetTranslationSpeed(float InSpeed) { m_TranslationSpeed = InSpeed; }

        float GetRotationSpeed() const { return m_RotationSpeed; }
        void SetRotationSpeed(float InSpeed) { m_RotationSpeed = InSpeed; }

        float GetGamepadRotationSpeed() const { return m_GamepadRotationSpeed; }
        void SetGamepadRotationSpeed(float InSpeed) { m_GamepadRotationSpeed = InSpeed; }

        float GetGamepadDeadzone() const { return m_GamepadDeadzone; }
        void SetGamepadDeadzone(float InDeadzone) { m_GamepadDeadzone = InDeadzone; }

    private:
        bool OnMouseScrolled(FMouseScrolledEvent& InEvent);
        bool OnWindowResized(FWindowResizeEvent& InEvent);

    private:
        FPerspectiveCamera m_Camera;

        float m_TranslationSpeed = 5.0f;
        float m_RotationSpeed = 0.12f;
        float m_GamepadRotationSpeed = 90.0f; // degrees per second
        float m_GamepadDeadzone = 0.15f;

        glm::vec2 m_LastMousePosition = {0.0f, 0.0f};
        bool bFirstMouse = true;
    };

    using PerspectiveCameraController = FPerspectiveCameraController;

} // namespace Leon
