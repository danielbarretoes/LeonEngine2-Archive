#include <doctest/doctest.h>
#include "GPU/HeadlessGLContext.hpp"
#include "Renderer/FShadowMath.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

using namespace Leon;

TEST_SUITE("Shader GPU - Shadow Cascaded Partitioning & Stabilization Math") {

    TEST_CASE("ShadowMath - Practical Split Scheme Calculations") {
        const float nearClip = 0.1f;
        const float farClip  = 100.0f;
        const uint32_t count = 4;

        SUBCASE("Practical Split Monotonicity and Boundary Invariants") {
            auto splits = ShadowMath::CalculateCascadeSplits(count, nearClip, farClip, 0.85f, ECascadeSplitScheme::Practical);
            REQUIRE(splits.size() == 5);

            CHECK(splits[0] == doctest::Approx(nearClip).epsilon(0.0001f));
            CHECK(splits[4] == doctest::Approx(farClip).epsilon(0.0001f));

            // Strictly increasing partition distances
            for (size_t i = 1; i < splits.size(); ++i) {
                CHECK(splits[i] > splits[i - 1]);
            }

            // Practical blend (0.85) should allocate tighter near-plane cascades than uniform
            auto uniformSplits = ShadowMath::CalculateCascadeSplits(count, nearClip, farClip, 0.0f, ECascadeSplitScheme::Uniform);
            CHECK(splits[1] < uniformSplits[1]);
            CHECK(splits[2] < uniformSplits[2]);
        }

        SUBCASE("Uniform Scheme Equidistant Spacing") {
            auto uniformSplits = ShadowMath::CalculateCascadeSplits(count, nearClip, farClip, 0.0f, ECascadeSplitScheme::Uniform);
            REQUIRE(uniformSplits.size() == 5);

            float expectedStep = (farClip - nearClip) / static_cast<float>(count);
            for (uint32_t i = 1; i <= count; ++i) {
                float expectedVal = nearClip + static_cast<float>(i) * expectedStep;
                CHECK(uniformSplits[i] == doctest::Approx(expectedVal).epsilon(0.001f));
            }
        }

        SUBCASE("Logarithmic Scheme High Near-Plane Density") {
            auto logSplits = ShadowMath::CalculateCascadeSplits(count, nearClip, farClip, 1.0f, ECascadeSplitScheme::Logarithmic);
            REQUIRE(logSplits.size() == 5);

            // Logarithmic split 1 should be dramatically closer to near plane
            CHECK(logSplits[1] < 1.0f);
            CHECK(logSplits[4] == doctest::Approx(farClip).epsilon(0.0001f));
        }
    }

    TEST_CASE("ShadowMath - Frustum Corner Extraction Invariants") {
        glm::mat4 proj = glm::perspective(glm::radians(60.0f), 16.0f / 9.0f, 0.1f, 50.0f);
        glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 2.0f, 5.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        auto corners = ShadowMath::GetFrustumCornersWorldSpace(proj, view);
        REQUIRE(corners.size() == 8);

        // Compute centroid
        glm::vec3 center(0.0f);
        for (const auto& c : corners) {
            center += c;
        }
        center /= 8.0f;

        // Centroid must lie in front of camera eye along view direction
        glm::vec3 camEye(0.0f, 2.0f, 5.0f);
        glm::vec3 viewDir = glm::normalize(glm::vec3(0.0f, 0.0f, 0.0f) - camEye);
        glm::vec3 toCenter = glm::normalize(center - camEye);
        CHECK(glm::dot(viewDir, toCenter) > 0.95f);
    }

    TEST_CASE("ShadowMath - Bounding Sphere Projection & Texel Grid Snapping") {
        glm::mat4 proj = glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 20.0f);
        glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        auto corners = ShadowMath::GetFrustumCornersWorldSpace(proj, view);

        glm::vec3 lightDir = glm::normalize(glm::vec3(0.5f, -1.0f, -0.3f));
        uint32_t resolution = 2048;

        float worldUnitsPerTexel = 0.0f;
        glm::mat4 stabilizedMatrix = ShadowMath::CalculateCascadeMatrix(corners, lightDir, resolution, true, worldUnitsPerTexel);

        CHECK(worldUnitsPerTexel > 0.0f);
        CHECK(worldUnitsPerTexel < 1.0f);

        // Test small translation jitter stability
        glm::mat4 jitteredView = glm::lookAt(glm::vec3(0.002f, 0.0f, 0.0f), glm::vec3(0.002f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        auto jitteredCorners = ShadowMath::GetFrustumCornersWorldSpace(proj, jitteredView);
        float jitteredTexelSize = 0.0f;
        glm::mat4 jitteredMatrix = ShadowMath::CalculateCascadeMatrix(jitteredCorners, lightDir, resolution, true, jitteredTexelSize);

        CHECK(jitteredTexelSize == doctest::Approx(worldUnitsPerTexel).epsilon(0.0001f));
    }

    TEST_CASE("ShadowMath - 2x2 Atlas Scale and Offset Coordinates") {
        auto q0 = ShadowMath::GetAtlasScaleOffset2x2(0);
        auto q1 = ShadowMath::GetAtlasScaleOffset2x2(1);
        auto q2 = ShadowMath::GetAtlasScaleOffset2x2(2);
        auto q3 = ShadowMath::GetAtlasScaleOffset2x2(3);

        CHECK(q0 == glm::vec4(0.5f, 0.5f, 0.0f, 0.0f));
        CHECK(q1 == glm::vec4(0.5f, 0.5f, 0.5f, 0.0f));
        CHECK(q2 == glm::vec4(0.5f, 0.5f, 0.0f, 0.5f));
        CHECK(q3 == glm::vec4(0.5f, 0.5f, 0.5f, 0.5f));
    }
}
