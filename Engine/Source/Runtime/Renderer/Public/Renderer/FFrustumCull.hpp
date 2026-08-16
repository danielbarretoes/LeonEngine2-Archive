#pragma once

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace Leon {

    /** Six frustum planes in ax+by+cz+d form (positive half-space = inside). */
    struct FFrustumPlanes {
        glm::vec4 Planes[6]{};
    };

    inline FFrustumPlanes ExtractFrustumPlanes(const glm::mat4& InViewProjection) {
        FFrustumPlanes f;
        const glm::mat4& m = InViewProjection;
        f.Planes[0] = glm::vec4(m[0][3] + m[0][0], m[1][3] + m[1][0], m[2][3] + m[2][0], m[3][3] + m[3][0]);
        f.Planes[1] = glm::vec4(m[0][3] - m[0][0], m[1][3] - m[1][0], m[2][3] - m[2][0], m[3][3] - m[3][0]);
        f.Planes[2] = glm::vec4(m[0][3] + m[0][1], m[1][3] + m[1][1], m[2][3] + m[2][1], m[3][3] + m[3][1]);
        f.Planes[3] = glm::vec4(m[0][3] - m[0][1], m[1][3] - m[1][1], m[2][3] - m[2][1], m[3][3] - m[3][1]);
        f.Planes[4] = glm::vec4(m[0][3] + m[0][2], m[1][3] + m[1][2], m[2][3] + m[2][2], m[3][3] + m[3][2]);
        f.Planes[5] = glm::vec4(m[0][3] - m[0][2], m[1][3] - m[1][2], m[2][3] - m[2][2], m[3][3] - m[3][2]);

        for (int i = 0; i < 6; ++i) {
            float len = glm::length(glm::vec3(f.Planes[i]));
            if (len > 1e-6f) {
                f.Planes[i] /= len;
            }
        }
        return f;
    }

    inline void TransformAABB(const glm::vec3& InLocalMin, const glm::vec3& InLocalMax, const glm::mat4& InWorld,
                              glm::vec3& OutWorldMin, glm::vec3& OutWorldMax) {
        const glm::vec3 corners[8] = {
            {InLocalMin.x, InLocalMin.y, InLocalMin.z}, {InLocalMax.x, InLocalMin.y, InLocalMin.z},
            {InLocalMin.x, InLocalMax.y, InLocalMin.z}, {InLocalMax.x, InLocalMax.y, InLocalMin.z},
            {InLocalMin.x, InLocalMin.y, InLocalMax.z}, {InLocalMax.x, InLocalMin.y, InLocalMax.z},
            {InLocalMin.x, InLocalMax.y, InLocalMax.z}, {InLocalMax.x, InLocalMax.y, InLocalMax.z},
        };
        OutWorldMin = glm::vec3(std::numeric_limits<float>::max());
        OutWorldMax = glm::vec3(std::numeric_limits<float>::lowest());
        for (const auto& c : corners) {
            glm::vec3 w = glm::vec3(InWorld * glm::vec4(c, 1.0f));
            OutWorldMin = glm::min(OutWorldMin, w);
            OutWorldMax = glm::max(OutWorldMax, w);
        }
    }

    /** True if AABB intersects or is inside the frustum. */
    inline bool AABBIntersectsFrustum(const glm::vec3& InMin, const glm::vec3& InMax, const FFrustumPlanes& InFrustum) {
        for (int i = 0; i < 6; ++i) {
            const glm::vec4& p = InFrustum.Planes[i];
            glm::vec3 positive = {
                p.x >= 0.0f ? InMax.x : InMin.x,
                p.y >= 0.0f ? InMax.y : InMin.y,
                p.z >= 0.0f ? InMax.z : InMin.z,
            };
            if (glm::dot(glm::vec3(p), positive) + p.w < 0.0f) {
                return false;
            }
        }
        return true;
    }

} // namespace Leon
