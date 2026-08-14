#include "renderer/PerspectiveCamera.hpp"
#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

namespace Leon {

    FPerspectiveCamera::FPerspectiveCamera(float InFOV, float InAspectRatio, float InNearClip, float InFarClip)
        : m_FOV(InFOV), m_AspectRatio(InAspectRatio), m_NearClip(InNearClip), m_FarClip(InFarClip) {
        RecalculateProjectionMatrix();
        RecalculateViewMatrix();
    }

    void FPerspectiveCamera::SetProjection(float InFOV, float InAspectRatio, float InNearClip, float InFarClip) {
        m_FOV = InFOV;
        m_AspectRatio = InAspectRatio;
        m_NearClip = InNearClip;
        m_FarClip = InFarClip;
        RecalculateProjectionMatrix();
    }

    void FPerspectiveCamera::SetViewportSize(uint32_t InWidth, uint32_t InHeight) {
        if (InHeight == 0)
            return;
        m_AspectRatio = static_cast<float>(InWidth) / static_cast<float>(InHeight);
        RecalculateProjectionMatrix();
    }

    void FPerspectiveCamera::SetRotation(float InPitch, float InYaw) {
        // Prevent gimbal lock
        m_Pitch = std::clamp(InPitch, -89.0f, 89.0f);
        m_Yaw = InYaw;
        RecalculateViewMatrix();
    }

    void FPerspectiveCamera::SetFOV(float InFOV) {
        m_FOV = std::clamp(InFOV, 10.0f, 120.0f);
        RecalculateProjectionMatrix();
    }

    glm::vec3 FPerspectiveCamera::GetForwardDirection() const {
        glm::vec3 forward;
        forward.x = std::cos(glm::radians(m_Yaw)) * std::cos(glm::radians(m_Pitch));
        forward.y = std::sin(glm::radians(m_Pitch));
        forward.z = std::sin(glm::radians(m_Yaw)) * std::cos(glm::radians(m_Pitch));
        return glm::normalize(forward);
    }

    glm::vec3 FPerspectiveCamera::GetRightDirection() const {
        return glm::normalize(glm::cross(GetForwardDirection(), glm::vec3(0.0f, 1.0f, 0.0f)));
    }

    glm::vec3 FPerspectiveCamera::GetUpDirection() const {
        return glm::normalize(glm::cross(GetRightDirection(), GetForwardDirection()));
    }

    void FPerspectiveCamera::RecalculateProjectionMatrix() {
        m_ProjectionMatrix = glm::perspective(glm::radians(m_FOV), m_AspectRatio, m_NearClip, m_FarClip);
        m_ViewProjectionMatrix = m_ProjectionMatrix * m_ViewMatrix;
    }

    void FPerspectiveCamera::RecalculateViewMatrix() {
        glm::vec3 forward = GetForwardDirection();
        m_ViewMatrix = glm::lookAt(m_Position, m_Position + forward, glm::vec3(0.0f, 1.0f, 0.0f));
        m_ViewProjectionMatrix = m_ProjectionMatrix * m_ViewMatrix;
    }

} // namespace Leon
