#pragma once

#include "Core/Base.hpp"
#include "Core/FTimestep.hpp"
#include "Core/events/FApplicationEvent.hpp"
#include "Core/events/FMouseEvent.hpp"
#include "Renderer/FPerspectiveCamera.hpp"

namespace Leon {

    class FPerspectiveCameraController {
    public:
        FPerspectiveCameraController(float InFOV = 45.0f, float InAspectRatio = 16.0f / 9.0f, float InNearClip = 0.1f,
                                     float InFarClip = 1000.0f);

        void OnUpdate(FTimestep InTs);
        void OnEvent(FEvent& InEvent);

        FPerspectiveCamera& GetCamera() { return Camera; }
        const FPerspectiveCamera& GetCamera() const { return Camera; }

        float GetTranslationSpeed() const { return TranslationSpeed; }
        void SetTranslationSpeed(float InSpeed) { TranslationSpeed = InSpeed; }

        float GetRotationSpeed() const { return RotationSpeed; }
        void SetRotationSpeed(float InSpeed) { RotationSpeed = InSpeed; }

        float GetGamepadRotationSpeed() const { return GamepadRotationSpeed; }
        void SetGamepadRotationSpeed(float InSpeed) { GamepadRotationSpeed = InSpeed; }

        float GetGamepadDeadzone() const { return GamepadDeadzone; }
        void SetGamepadDeadzone(float InDeadzone) { GamepadDeadzone = InDeadzone; }

    private:
        bool OnMouseScrolled(FMouseScrolledEvent& InEvent);
        bool OnWindowResized(FWindowResizeEvent& InEvent);

    private:
        FPerspectiveCamera Camera;

        float TranslationSpeed = 5.0f;
        float RotationSpeed = 0.12f;
        float GamepadRotationSpeed = 90.0f; // degrees per second
        float GamepadDeadzone = 0.15f;

        glm::vec2 LastMousePosition = {0.0f, 0.0f};
        bool bFirstMouse = true;
    };

} // namespace Leon
