#pragma once

#include "Core/Base.hpp"
#include <glm/glm.hpp>

namespace Leon {

    class FPerspectiveCamera {
    public:
        FPerspectiveCamera(float InFOV = 45.0f, float InAspectRatio = 16.0f / 9.0f, float InNearClip = 0.1f,
                           float InFarClip = 1000.0f);

        void SetProjection(float InFOV, float InAspectRatio, float InNearClip, float InFarClip);
        void SetViewportSize(uint32_t InWidth, uint32_t InHeight);

        const glm::vec3& GetPosition() const { return Position; }
        void SetPosition(const glm::vec3& InPosition) {
            Position = InPosition;
            RecalculateViewMatrix();
        }

        float GetPitch() const { return Pitch; }
        float GetYaw() const { return Yaw; }
        void SetRotation(float InPitch, float InYaw);

        float GetFOV() const { return FOV; }
        void SetFOV(float InFOV);

        float GetAspectRatio() const { return AspectRatio; }
        float GetNearClip() const { return NearClip; }
        float GetFarClip() const { return FarClip; }

        const glm::mat4& GetProjectionMatrix() const { return ProjectionMatrix; }
        const glm::mat4& GetViewMatrix() const { return ViewMatrix; }
        const glm::mat4& GetViewProjectionMatrix() const { return ViewProjectionMatrix; }

        glm::vec3 GetForwardDirection() const;
        glm::vec3 GetRightDirection() const;
        glm::vec3 GetUpDirection() const;

    private:
        void RecalculateProjectionMatrix();
        void RecalculateViewMatrix();

    private:
        float FOV = 45.0f;
        float AspectRatio = 1.777778f;
        float NearClip = 0.1f;
        float FarClip = 1000.0f;

        glm::vec3 Position = {0.0f, 0.0f, 3.0f};
        float Pitch = 0.0f;
        float Yaw = -90.0f;

        glm::mat4 ProjectionMatrix = glm::mat4(1.0f);
        glm::mat4 ViewMatrix = glm::mat4(1.0f);
        glm::mat4 ViewProjectionMatrix = glm::mat4(1.0f);
    };

} // namespace Leon
