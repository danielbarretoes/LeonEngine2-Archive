#pragma once

#include <glm/glm.hpp>
#include <cmath>

namespace Leon {

    /**
     * World unit and axis contract.
     *
     * 1 world unit = 1 meter. Actor transforms, physics, camera arm lengths,
     * and movement speeds are metres / metres-per-second.
     * FBX centimetres are converted at import (ufbx target_unit_meters = 1).
     *
     * Right-handed Y-up (OpenGL / GLM):
     *   +X = Right
     *   +Y = Up
     *   -Z = Forward
     *
     * Mixamo / many character FBX files are authored +Z forward. USkeletalMesh
     * AssetForwardAxis maps source +Z onto engine -Z at the mesh component —
     * not via ad-hoc gameplay yaw.
     */
    struct FWorldUnits {
        static constexpr float MetersPerUnit = 1.0f;
        static constexpr float UnitsPerMeter = 1.0f;
        static constexpr float CentimetresPerUnit = 100.0f;

        static constexpr float ExpectedHumanHeightMin = 1.5f;
        static constexpr float ExpectedHumanHeightMax = 2.1f;
        static constexpr float ExpectedEyeHeight = 1.7f;
        static constexpr float ExpectedCapsuleRadius = 0.4f;

        static constexpr glm::vec3 Right() { return {1.0f, 0.0f, 0.0f}; }
        static constexpr glm::vec3 Up() { return {0.0f, 1.0f, 0.0f}; }
        static constexpr glm::vec3 Forward() { return {0.0f, 0.0f, -1.0f}; }

        static glm::vec3 PlanarForwardFromYaw(float InYawDegrees) {
            const float yaw = glm::radians(InYawDegrees);
            glm::vec3 f(std::cos(yaw), 0.0f, std::sin(yaw));
            float len = glm::length(f);
            return len > 1e-6f ? f / len : Forward();
        }

        static glm::vec3 LookDirection(float InPitchDegrees, float InYawDegrees) {
            const float pitch = glm::radians(InPitchDegrees);
            const float yaw = glm::radians(InYawDegrees);
            glm::vec3 look;
            look.x = std::cos(yaw) * std::cos(pitch);
            look.y = std::sin(pitch);
            look.z = std::sin(yaw) * std::cos(pitch);
            float len = glm::length(look);
            return len > 1e-6f ? look / len : Forward();
        }
    };

} // namespace Leon
