#include "renderer/ShadowMath.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

namespace Leon {

    namespace ShadowMath {

        std::vector<float> CalculateCascadeSplits(uint32_t InCount, float InNearClip, float InFarClip, float InLambda,
                                                  ECascadeSplitScheme InScheme) {
            if (InCount == 0)
                return {InNearClip, InFarClip};

            std::vector<float> splits(InCount + 1);
            splits[0] = InNearClip;
            splits[InCount] = InFarClip;

            float lambda = InLambda;
            if (InScheme == ECascadeSplitScheme::Logarithmic) {
                lambda = 1.0f;
            } else if (InScheme == ECascadeSplitScheme::Uniform) {
                lambda = 0.0f;
            }
            lambda = std::clamp(lambda, 0.0f, 1.0f);

            float nearFarRatio = InFarClip / std::max(InNearClip, 0.0001f);
            float clipRange = InFarClip - InNearClip;

            for (uint32_t i = 1; i < InCount; ++i) {
                float p = static_cast<float>(i) / static_cast<float>(InCount);
                float logSplit = InNearClip * std::pow(nearFarRatio, p);
                float uniformSplit = InNearClip + clipRange * p;
                splits[i] = lambda * logSplit + (1.0f - lambda) * uniformSplit;
            }

            return splits;
        }

        std::array<glm::vec3, 8> GetFrustumCornersWorldSpace(const glm::mat4& InProj, const glm::mat4& InView) {
            glm::mat4 invVP = glm::inverse(InProj * InView);
            std::array<glm::vec3, 8> corners{};

            uint32_t idx = 0;
            for (int x = 0; x < 2; ++x) {
                for (int y = 0; y < 2; ++y) {
                    for (int z = 0; z < 2; ++z) {
                        glm::vec4 pt =
                            invVP * glm::vec4(2.0f * static_cast<float>(x) - 1.0f, 2.0f * static_cast<float>(y) - 1.0f,
                                              2.0f * static_cast<float>(z) - 1.0f, 1.0f);
                        corners[idx++] = glm::vec3(pt / pt.w);
                    }
                }
            }

            return corners;
        }

        glm::mat4 CalculateCascadeMatrix(const std::array<glm::vec3, 8>& InFrustumCorners, const glm::vec3& InLightDir,
                                         uint32_t InResolution, bool bInStabilize, float& OutWorldUnitsPerTexel) {
            glm::vec3 lightDirNorm = glm::normalize(InLightDir);
            glm::vec3 up =
                (std::abs(lightDirNorm.y) < 0.99f) ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(0.0f, 0.0f, 1.0f);

            // Compute geometric centroid of the frustum slice
            glm::vec3 center(0.0f);
            for (const auto& v : InFrustumCorners) {
                center += v;
            }
            center /= 8.0f;

            if (bInStabilize) {
                // Compute minimum bounding sphere enclosing all 8 corners
                float radius = 0.0f;
                for (const auto& v : InFrustumCorners) {
                    float dist = glm::length(v - center);
                    radius = std::max(radius, dist);
                }
                radius = std::ceil(radius * 16.0f) / 16.0f; // Quantize radius to prevent precision jitter

                float texelResolution = static_cast<float>(InResolution > 0 ? InResolution : 2048);
                float worldUnitsPerTexel = (2.0f * radius) / texelResolution;
                OutWorldUnitsPerTexel = worldUnitsPerTexel;

                // Create provisional light view centered at world origin
                glm::mat4 lightViewZero = glm::lookAt(glm::vec3(0.0f), lightDirNorm, up);
                glm::vec3 centerLS = glm::vec3(lightViewZero * glm::vec4(center, 1.0f));

                // Snap center in light space to exact texel grid boundary
                centerLS.x = std::floor(centerLS.x / worldUnitsPerTexel) * worldUnitsPerTexel;
                centerLS.y = std::floor(centerLS.y / worldUnitsPerTexel) * worldUnitsPerTexel;

                // Transform snapped center back to world space
                glm::mat4 invLightViewZero = glm::inverse(lightViewZero);
                glm::vec3 snappedCenterWS = glm::vec3(invLightViewZero * glm::vec4(centerLS, 1.0f));

                // Position light back from snapped center to capture all potential shadow casters
                float zMargin = radius * 4.0f;
                glm::vec3 lightPos = snappedCenterWS - lightDirNorm * zMargin;
                glm::mat4 lightView = glm::lookAt(lightPos, snappedCenterWS, up);

                // Symmetrical orthographic bounds aligned with bounding sphere radius
                float nearPlane = 0.1f;
                float farPlane = zMargin * 2.0f;
                glm::mat4 lightProj = glm::ortho(-radius, radius, -radius, radius, nearPlane, farPlane);

                return lightProj * lightView;
            } else {
                // Non-stabilized standard AABB fitting
                glm::vec3 lightPos = center - lightDirNorm * 40.0f;
                glm::mat4 lightView = glm::lookAt(lightPos, center, up);

                float minX = std::numeric_limits<float>::max(), maxX = std::numeric_limits<float>::lowest();
                float minY = std::numeric_limits<float>::max(), maxY = std::numeric_limits<float>::lowest();
                float minZ = std::numeric_limits<float>::max(), maxZ = std::numeric_limits<float>::lowest();

                for (const auto& v : InFrustumCorners) {
                    glm::vec4 trf = lightView * glm::vec4(v, 1.0f);
                    minX = std::min(minX, trf.x);
                    maxX = std::max(maxX, trf.x);
                    minY = std::min(minY, trf.y);
                    maxY = std::max(maxY, trf.y);
                    minZ = std::min(minZ, trf.z);
                    maxZ = std::max(maxZ, trf.z);
                }

                float zMargin = 30.0f;
                minZ -= zMargin;
                maxZ += zMargin;

                float texelResolution = static_cast<float>(InResolution > 0 ? InResolution : 2048);
                float worldUnitsPerTexelX = (maxX - minX) / texelResolution;
                minX = std::floor(minX / worldUnitsPerTexelX) * worldUnitsPerTexelX;
                maxX = minX + texelResolution * worldUnitsPerTexelX;

                float worldUnitsPerTexelY = (maxY - minY) / texelResolution;
                minY = std::floor(minY / worldUnitsPerTexelY) * worldUnitsPerTexelY;
                maxY = minY + texelResolution * worldUnitsPerTexelY;

                OutWorldUnitsPerTexel = std::max(worldUnitsPerTexelX, worldUnitsPerTexelY);

                float nearPlane = -maxZ;
                float farPlane = -minZ;
                if (nearPlane > farPlane)
                    std::swap(nearPlane, farPlane);
                if (nearPlane < 0.1f)
                    nearPlane = 0.1f;

                glm::mat4 lightProj = glm::ortho(minX, maxX, minY, maxY, nearPlane, farPlane);
                return lightProj * lightView;
            }
        }

        glm::vec4 GetAtlasScaleOffset2x2(uint32_t InCascadeIndex) {
            switch (InCascadeIndex) {
            case 0:
                return glm::vec4(0.5f, 0.5f, 0.0f, 0.0f);
            case 1:
                return glm::vec4(0.5f, 0.5f, 0.5f, 0.0f);
            case 2:
                return glm::vec4(0.5f, 0.5f, 0.0f, 0.5f);
            case 3:
            default:
                return glm::vec4(0.5f, 0.5f, 0.5f, 0.5f);
            }
        }

    } // namespace ShadowMath

} // namespace Leon
