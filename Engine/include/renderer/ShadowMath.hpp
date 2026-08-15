#pragma once

#include "renderer/ShadowTypes.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <array>

namespace Leon {

    namespace ShadowMath {

        /**
         * @brief Calculates partition distances for Cascaded Shadow Maps.
         *
         * @param InCount Number of cascade slices (e.g. 4)
         * @param InNearClip Camera near clipping distance (e.g. 0.1f)
         * @param InFarClip Maximum shadow distance (e.g. 100.0f)
         * @param InLambda Practical split scheme blending factor in [0.0, 1.0] (0 = uniform, 1 = logarithmic)
         * @param InScheme Split scheme selection (Practical, Logarithmic, or Uniform)
         * @return std::vector<float> Vector of (InCount + 1) distance boundaries
         */
        std::vector<float> CalculateCascadeSplits(uint32_t InCount, float InNearClip, float InFarClip,
                                                  float InLambda = 0.85f,
                                                  ECascadeSplitScheme InScheme = ECascadeSplitScheme::Practical);

        /**
         * @brief Computes the 8 world-space corners of a camera frustum slice.
         */
        std::array<glm::vec3, 8> GetFrustumCornersWorldSpace(const glm::mat4& InProj, const glm::mat4& InView);

        /**
         * @brief Computes an orthographic light projection matrix fitted and stabilized to a frustum slice.
         *
         * @param InFrustumCorners 8 world-space corners of the slice
         * @param InLightDir Normalized direction towards which light is traveling
         * @param InResolution Texture resolution along width/height for texel grid snapping
         * @param bInStabilize When true, locks bounding sphere and snaps projection center to texel increments
         * @param OutWorldUnitsPerTexel Outputs the world-space size of a single shadow texel
         * @return glm::mat4 Combined LightProjection * LightView matrix
         */
        glm::mat4 CalculateCascadeMatrix(const std::array<glm::vec3, 8>& InFrustumCorners, const glm::vec3& InLightDir,
                                         uint32_t InResolution, bool bInStabilize, float& OutWorldUnitsPerTexel);

        /**
         * @brief Computes UV scale and offset for a 2x2 atlas partition.
         */
        glm::vec4 GetAtlasScaleOffset2x2(uint32_t InCascadeIndex);

    } // namespace ShadowMath

} // namespace Leon
