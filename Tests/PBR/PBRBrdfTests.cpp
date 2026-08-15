#include <doctest/doctest.h>
#include <vector>
#include "renderer/PBRMath.hpp"

TEST_SUITE("PBR - Cook-Torrance Microfacet BRDF Invariants") {

    TEST_CASE("Trowbridge-Reitz GGX NDF Non-Negativity and Extreme Roughness Handling") {
        std::vector<float> testRoughness = {0.001f, 0.05f, 0.25f, 0.5f, 0.75f, 1.0f};
        std::vector<float> testNdotH = {0.0f, 0.25f, 0.5f, 0.75f, 0.999f, 1.0f};

        for (float r : testRoughness) {
            for (float ndoth : testNdotH) {
                float ndf = Leon::DistributionGGX(ndoth, r);
                CHECK(ndf >= 0.0f);
                CHECK(!std::isnan(ndf));
            }
        }

        // Exact Analytical Numerical Reference Checks:
        // roughness = 0.5 -> a = 0.25, a2 = 0.0625
        // NdotH = 1.0 -> D = 1 / (PI * a2) = 1 / (PI * 0.0625) ~= 5.092958
        // NdotH = 0.0 -> D = a2 / PI = 0.0625 / PI ~= 0.019894
        CHECK(Leon::DistributionGGX(1.0f, 0.5f) == doctest::Approx(5.092958f).epsilon(0.001f));
        CHECK(Leon::DistributionGGX(0.0f, 0.5f) == doctest::Approx(0.019894f).epsilon(0.001f));
    }

    TEST_CASE("Smith Schlick-GGX Geometry Function in Range [0, 1]") {
        std::vector<float> testRoughness = {0.001f, 0.1f, 0.5f, 1.0f};
        std::vector<float> testAngles = {0.0001f, 0.2f, 0.5f, 0.8f, 1.0f};

        for (float r : testRoughness) {
            for (float nv : testAngles) {
                for (float nl : testAngles) {
                    float g = Leon::GeometrySmith_Direct(nv, nl, r);
                    CHECK(g >= 0.0f);
                    CHECK(g <= 1.00001f);
                    CHECK(!std::isnan(g));
                }
            }
        }

        // When viewing straight down on smooth surface, shadowing is zero (G = 1)
        float gPerp = Leon::GeometrySmith_Direct(1.0f, 1.0f, 0.001f);
        CHECK(gPerp == doctest::Approx(1.0f).epsilon(0.01f));

        // Exact Analytical Numerical Reference Check:
        // roughness = 0.5 -> r = 1.5, k = 2.25 / 8 = 0.28125
        // NdotV = 0.5, NdotL = 0.5 -> G1 = 0.5 / (0.5 * 0.71875 + 0.28125) = 0.7804878
        // G = G1 * G1 = 0.609161
        CHECK(Leon::GeometrySmith_Direct(0.5f, 0.5f, 0.5f) == doctest::Approx(0.609161f).epsilon(0.001f));
    }

    TEST_CASE("Fresnel-Schlick Boundary Conditions F(0) = F0 and F(pi/2) = 1.0") {
        glm::vec3 F0_dielectric(0.04f);
        glm::vec3 F0_gold(1.00f, 0.71f, 0.29f);

        // 1. Perpendicular incidence (cosTheta = 1.0) -> F == F0
        glm::vec3 fDiel0 = Leon::FresnelSchlick(1.0f, F0_dielectric);
        CHECK(fDiel0.r == doctest::Approx(0.04f).epsilon(1e-5f));
        CHECK(fDiel0.g == doctest::Approx(0.04f).epsilon(1e-5f));
        CHECK(fDiel0.b == doctest::Approx(0.04f).epsilon(1e-5f));

        glm::vec3 fGold0 = Leon::FresnelSchlick(1.0f, F0_gold);
        CHECK(fGold0.r == doctest::Approx(1.00f).epsilon(1e-5f));
        CHECK(fGold0.g == doctest::Approx(0.71f).epsilon(1e-5f));
        CHECK(fGold0.b == doctest::Approx(0.29f).epsilon(1e-5f));

        // 2. Exact Oblique Angle (cosTheta = 0.5) -> (1-0.5)^5 = 0.03125
        // F = 0.04 + 0.96 * 0.03125 = 0.07000 (catches exponent mutations 4.0 vs 5.0)
        glm::vec3 fDielMid = Leon::FresnelSchlick(0.5f, F0_dielectric);
        CHECK(fDielMid.r == doctest::Approx(0.07000f).epsilon(1e-4f));
        CHECK(fDielMid.g == doctest::Approx(0.07000f).epsilon(1e-4f));
        CHECK(fDielMid.b == doctest::Approx(0.07000f).epsilon(1e-4f));

        // 3. Glancing incidence (cosTheta = 0.0) -> F == 1.0 (all materials become 100% reflective)
        glm::vec3 fDielGlance = Leon::FresnelSchlick(0.0f, F0_dielectric);
        CHECK(fDielGlance.r == doctest::Approx(1.0f).epsilon(1e-5f));
        CHECK(fDielGlance.g == doctest::Approx(1.0f).epsilon(1e-5f));
        CHECK(fDielGlance.b == doctest::Approx(1.0f).epsilon(1e-5f));

        glm::vec3 fGoldGlance = Leon::FresnelSchlick(0.0f, F0_gold);
        CHECK(fGoldGlance.r == doctest::Approx(1.0f).epsilon(1e-5f));
        CHECK(fGoldGlance.g == doctest::Approx(1.0f).epsilon(1e-5f));
        CHECK(fGoldGlance.b == doctest::Approx(1.0f).epsilon(1e-5f));
    }
}
