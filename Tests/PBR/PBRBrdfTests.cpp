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
                CHECK(!std::isinf(ndf));
            }
        }
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

        // 2. Glancing incidence (cosTheta = 0.0) -> F == 1.0 (all materials become 100% reflective)
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
