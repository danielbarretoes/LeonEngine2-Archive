#include "gameplay/ADefaultPawn.hpp"
#include <algorithm>
#include <cmath>

namespace Leon {

    ADefaultPawn::ADefaultPawn(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : APawn(InHandle, InWorld, InName) {}

    void ADefaultPawn::PostInitializeComponents() {
        if (!HasComponent<UCameraComponent>()) {
            FPerspectiveCamera camera(45.0f, 1280.0f / 720.0f, 0.1f, 1000.0f);
            AddComponent<UCameraComponent>(camera);
        }
    }

    void ADefaultPawn::Tick(float DeltaSeconds) {
        if (IsControlled()) {
            SetupPlayerInputComponent(DeltaSeconds);
        }
    }

    void ADefaultPawn::SetupPlayerInputComponent(float DeltaSeconds) {
        auto& transform = GetTransform();
        glm::vec3& position = transform.Translation;

        // Right-click look navigation
        if (FInput::IsMouseButtonPressed(Mouse::ButtonRight)) {
            auto [mx, my] = FInput::GetMousePosition();
            if (m_bFirstMouse) {
                m_LastMousePos = {mx, my};
                m_bFirstMouse = false;
            }

            float offsetX = (mx - m_LastMousePos.x) * m_LookSensitivity;
            float offsetY = (m_LastMousePos.y - my) * m_LookSensitivity;
            m_LastMousePos = {mx, my};

            m_Yaw += offsetX;
            m_Pitch += offsetY;
            m_Pitch = std::clamp(m_Pitch, -89.0f, 89.0f);
        } else {
            m_bFirstMouse = true;
        }

        // Direction vectors
        glm::vec3 front;
        front.x = std::cos(glm::radians(m_Yaw)) * std::cos(glm::radians(m_Pitch));
        front.y = std::sin(glm::radians(m_Pitch));
        front.z = std::sin(glm::radians(m_Yaw)) * std::cos(glm::radians(m_Pitch));
        front = glm::normalize(front);

        glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
        glm::vec3 up = glm::normalize(glm::cross(right, front));

        // Movement
        float speed = m_MoveSpeed;
        if (FInput::IsKeyPressed(Key::LeftShift)) {
            speed *= m_SprintMultiplier;
        }

        float delta = speed * DeltaSeconds;

        if (FInput::IsKeyPressed(Key::W)) position += front * delta;
        if (FInput::IsKeyPressed(Key::S)) position -= front * delta;
        if (FInput::IsKeyPressed(Key::A)) position -= right * delta;
        if (FInput::IsKeyPressed(Key::D)) position += right * delta;
        if (FInput::IsKeyPressed(Key::E) || FInput::IsKeyPressed(Key::Space)) position += up * delta;
        if (FInput::IsKeyPressed(Key::Q) || FInput::IsKeyPressed(Key::LeftControl)) position -= up * delta;

        // Update camera component
        if (HasComponent<UCameraComponent>()) {
            auto& camComp = GetComponent<UCameraComponent>();
            camComp.Camera.SetPosition(position);
            camComp.Camera.SetRotation(m_Pitch, m_Yaw);
        }

        transform.Rotation = {m_Pitch, m_Yaw, 0.0f};
    }

} // namespace Leon
