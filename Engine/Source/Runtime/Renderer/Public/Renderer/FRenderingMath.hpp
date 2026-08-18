#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/matrix.hpp>
#include <algorithm>
#include <cmath>

namespace Leon {

    inline constexpr float kRenderingEpsilon = 1e-8f;

    inline glm::vec3 SafeNormalize(const glm::vec3& InV, const glm::vec3& InFallback = glm::vec3(0.0f, 1.0f, 0.0f)) {
        float len2 = glm::dot(InV, InV);
        if (len2 < kRenderingEpsilon)
            return InFallback;
        return InV * (1.0f / std::sqrt(len2));
    }

    /** Inverse-transpose of the upper 3x3. Identity if the matrix is singular. */
    inline glm::mat3 SafeNormalMatrix(const glm::mat4& InModel) {
        glm::mat3 m(InModel);
        float det = glm::determinant(m);
        if (std::abs(det) < 1e-12f)
            return glm::mat3(1.0f);
        return glm::transpose(glm::inverse(m));
    }

    inline bool HasNegativeScale(const glm::mat4& InModel) {
        return glm::determinant(glm::mat3(InModel)) < 0.0f;
    }

    /**
     * Stable camera basis for RH Y-up, camera looking along InForward.
     * Avoids NaNs when InForward is parallel to world +Y.
     */
    inline void StableViewBasis(const glm::vec3& InForward, glm::vec3& OutRight, glm::vec3& OutUp) {
        glm::vec3 forward = SafeNormalize(InForward, glm::vec3(0.0f, 0.0f, -1.0f));
        glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
        glm::vec3 right = glm::cross(forward, worldUp);
        if (glm::dot(right, right) < 1e-8f)
            right = glm::cross(forward, glm::vec3(0.0f, 0.0f, 1.0f));
        if (glm::dot(right, right) < 1e-8f)
            right = glm::vec3(1.0f, 0.0f, 0.0f);
        OutRight = glm::normalize(right);
        OutUp = glm::normalize(glm::cross(OutRight, forward));
    }

    /**
     * Actor Euler that maps local +Y (cylinder barrel axis) to InForward.
     */
    inline glm::vec3 EulerAligningLocalY(const glm::vec3& InForward) {
        glm::vec3 right, up;
        StableViewBasis(InForward, right, up);
        glm::vec3 forward = SafeNormalize(InForward, glm::vec3(0.0f, 0.0f, -1.0f));
        glm::mat3 rotation(right, forward, up);
        glm::quat q = glm::normalize(glm::quat_cast(rotation));
        return glm::degrees(glm::eulerAngles(q));
    }

    /** Plane n·x + Distance = 0 (n unit). Householder reflection through that plane. */
    inline glm::mat4 PlanarReflectionMatrix(const glm::vec3& InNormal, float InDistance) {
        glm::vec3 n = SafeNormalize(InNormal, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat3 r = glm::mat3(1.0f) - 2.0f * glm::outerProduct(n, n);
        glm::mat4 m(1.0f);
        m[0] = glm::vec4(r[0], 0.0f);
        m[1] = glm::vec4(r[1], 0.0f);
        m[2] = glm::vec4(r[2], 0.0f);
        m[3] = glm::vec4(-2.0f * InDistance * n, 1.0f);
        return m;
    }

    inline glm::vec3 ReflectPointThroughPlane(const glm::vec3& InPoint, const glm::vec3& InNormal, float InDistance) {
        glm::vec3 n = SafeNormalize(InNormal, glm::vec3(0.0f, 1.0f, 0.0f));
        return InPoint - 2.0f * (glm::dot(n, InPoint) + InDistance) * n;
    }

    inline bool IsHorizontalPlanarPlane(const glm::vec3& InNormal) {
        return std::abs(SafeNormalize(InNormal, glm::vec3(0.0f, 1.0f, 0.0f)).y) > 0.85f;
    }

    /** Wall capture only when the camera is reasonably facing that plane. */
    inline constexpr float kWallPlanarCaptureMinScore = 0.45f;

    /**
     * Higher score wins among candidate planes.
     * Camera on the back side of the plane is rejected so wall mirrors only fire from the interior.
     */
    inline float PlanarReflectionPlaneScore(const glm::vec3& InNormal, float InDistance,
                                            const glm::vec3& InCameraPosition, const glm::vec3& InCameraForward) {
        glm::vec3 n = SafeNormalize(InNormal, glm::vec3(0.0f, 1.0f, 0.0f));
        const float side = glm::dot(n, InCameraPosition) + InDistance;
        if (side < 0.05f)
            return -1.0e9f;
        const float lookingAt = -glm::dot(InCameraForward, n);
        float score = lookingAt;
        if (std::abs(n.y) > 0.85f)
            score += std::max(0.0f, -InCameraForward.y) * 1.35f;
        else
            score += std::max(0.0f, lookingAt) * 0.4f;
        return score;
    }

    /**
     * Actor Euler (degrees, Pitch/Yaw/Roll = GLM XYZ) that maps local -Z to InForward.
     * Matches FPerspectiveCamera look direction so pawn meshes face the camera heading.
     */
    inline glm::vec3 EulerLookingAlong(const glm::vec3& InForward) {
        glm::vec3 right, up;
        StableViewBasis(InForward, right, up);
        glm::mat3 rotation(right, up, -InForward);
        glm::quat q = glm::normalize(glm::quat_cast(rotation));
        glm::vec3 radians = glm::eulerAngles(q);
        return glm::degrees(radians);
    }

} // namespace Leon
