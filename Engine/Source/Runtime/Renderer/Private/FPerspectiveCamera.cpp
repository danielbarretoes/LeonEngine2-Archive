#include "Renderer/FPerspectiveCamera.hpp"
#include "Renderer/FRenderingMath.hpp"
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
        bOrthographic = false;
        RecalculateProjectionMatrix();
    }

    void FPerspectiveCamera::SetOrthographic(bool bInOrthographic, float InHeight) {
        bOrthographic = bInOrthographic;
        OrthoHeight = std::max(InHeight, 0.25f);
        RecalculateProjectionMatrix();
    }

    void FPerspectiveCamera::SetOrthoHeight(float InHeight) {
        OrthoHeight = std::clamp(InHeight, 0.25f, 500.0f);
        if (bOrthographic)
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
        return SafeNormalize(forward, glm::vec3(0.0f, 0.0f, -1.0f));
    }

    glm::vec3 FPerspectiveCamera::GetRightDirection() const {
        glm::vec3 right, up;
        StableViewBasis(GetForwardDirection(), right, up);
        return right;
    }

    glm::vec3 FPerspectiveCamera::GetUpDirection() const {
        glm::vec3 right, up;
        StableViewBasis(GetForwardDirection(), right, up);
        return up;
    }

    void FPerspectiveCamera::RecalculateProjectionMatrix() {
        float aspect = std::max(AspectRatio, 1e-4f);
        if (bOrthographic) {
            const float halfH = std::max(OrthoHeight, 0.25f) * 0.5f;
            const float halfW = halfH * aspect;
            ProjectionMatrix = glm::ortho(-halfW, halfW, -halfH, halfH, NearClip, FarClip);
        } else {
            ProjectionMatrix = glm::perspective(glm::radians(FOV), aspect, NearClip, FarClip);
        }
        ViewProjectionMatrix = ProjectionMatrix * ViewMatrix;
    }

    void FPerspectiveCamera::RecalculateViewMatrix() {
        glm::vec3 forward = GetForwardDirection();
        glm::vec3 right, up;
        StableViewBasis(forward, right, up);
        ViewMatrix = glm::lookAt(Position, Position + forward, up);
        ViewProjectionMatrix = ProjectionMatrix * ViewMatrix;
    }

} // namespace Leon
