#pragma once

#include "engine/core/Base.hpp"
#include <glm/glm.hpp>

namespace Leon {

    class FPerspectiveCamera {
    public:
        FPerspectiveCamera(float InFOV = 45.0f, float InAspectRatio = 16.0f / 9.0f, float InNearClip = 0.1f,
                           float InFarClip = 1000.0f);

        void SetProjection(float InFOV, float InAspectRatio, float InNearClip, float InFarClip);
        void SetViewportSize(uint32_t InWidth, uint32_t InHeight);

        const glm::vec3& GetPosition() const { return m_Position; }
        void SetPosition(const glm::vec3& InPosition) {
            m_Position = InPosition;
            RecalculateViewMatrix();
        }

        float GetPitch() const { return m_Pitch; }
        float GetYaw() const { return m_Yaw; }
        void SetRotation(float InPitch, float InYaw);

        float GetFOV() const { return m_FOV; }
        void SetFOV(float InFOV);

        const glm::mat4& GetProjectionMatrix() const { return m_ProjectionMatrix; }
        const glm::mat4& GetViewMatrix() const { return m_ViewMatrix; }
        const glm::mat4& GetViewProjectionMatrix() const { return m_ViewProjectionMatrix; }

        glm::vec3 GetForwardDirection() const;
        glm::vec3 GetRightDirection() const;
        glm::vec3 GetUpDirection() const;

    private:
        void RecalculateProjectionMatrix();
        void RecalculateViewMatrix();

    private:
        float m_FOV = 45.0f;
        float m_AspectRatio = 1.777778f;
        float m_NearClip = 0.1f;
        float m_FarClip = 1000.0f;

        glm::vec3 m_Position = {0.0f, 0.0f, 3.0f};
        float m_Pitch = 0.0f;
        float m_Yaw = -90.0f;

        glm::mat4 m_ProjectionMatrix = glm::mat4(1.0f);
        glm::mat4 m_ViewMatrix = glm::mat4(1.0f);
        glm::mat4 m_ViewProjectionMatrix = glm::mat4(1.0f);
    };

    using PerspectiveCamera = FPerspectiveCamera;

} // namespace Leon
