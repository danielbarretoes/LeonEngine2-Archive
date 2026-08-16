#include "Renderer/FPerspectiveCamera.hpp"
#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

namespace Leon {

    FPerspectiveCamera::FPerspectiveCamera(float InFOV, float InAspectRatio, float InNearClip, float InFarClip)
        : FOV(InFOV), AspectRatio(InAspectRatio), NearClip(InNearClip), FarClip(InFarClip) {
        RecalculateProjectionMatrix();
        RecalculateViewMatrix();
    }

    void FPerspectiveCamera::SetProjection(float InFOV, float InAspectRatio, float InNearClip, float InFarClip) {
        FOV = InFOV;
        AspectRatio = InAspectRatio;
        NearClip = InNearClip;
        FarClip = InFarClip;
        RecalculateProjectionMatrix();
    }

    void FPerspectiveCamera::SetViewportSize(uint32_t InWidth, uint32_t InHeight) {
        if (InHeight == 0)
            return;
        AspectRatio = static_cast<float>(InWidth) / static_cast<float>(InHeight);
        RecalculateProjectionMatrix();
    }

    void FPerspectiveCamera::SetRotation(float InPitch, float InYaw) {
        // Prevent gimbal lock
        Pitch = std::clamp(InPitch, -89.0f, 89.0f);
        Yaw = InYaw;
        RecalculateViewMatrix();
    }

    void FPerspectiveCamera::SetFOV(float InFOV) {
        FOV = std::clamp(InFOV, 10.0f, 120.0f);
        RecalculateProjectionMatrix();
    }

    glm::vec3 FPerspectiveCamera::GetForwardDirection() const {
        glm::vec3 forward;
        forward.x = std::cos(glm::radians(Yaw)) * std::cos(glm::radians(Pitch));
        forward.y = std::sin(glm::radians(Pitch));
        forward.z = std::sin(glm::radians(Yaw)) * std::cos(glm::radians(Pitch));
        return glm::normalize(forward);
    }

    glm::vec3 FPerspectiveCamera::GetRightDirection() const {
        return glm::normalize(glm::cross(GetForwardDirection(), glm::vec3(0.0f, 1.0f, 0.0f)));
    }

    glm::vec3 FPerspectiveCamera::GetUpDirection() const {
        return glm::normalize(glm::cross(GetRightDirection(), GetForwardDirection()));
    }

    void FPerspectiveCamera::RecalculateProjectionMatrix() {
        ProjectionMatrix = glm::perspective(glm::radians(FOV), AspectRatio, NearClip, FarClip);
        ViewProjectionMatrix = ProjectionMatrix * ViewMatrix;
    }

    void FPerspectiveCamera::RecalculateViewMatrix() {
        glm::vec3 forward = GetForwardDirection();
        ViewMatrix = glm::lookAt(Position, Position + forward, glm::vec3(0.0f, 1.0f, 0.0f));
        ViewProjectionMatrix = ProjectionMatrix * ViewMatrix;
    }

} // namespace Leon
