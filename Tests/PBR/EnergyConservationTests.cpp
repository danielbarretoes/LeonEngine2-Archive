#include <doctest/doctest.h>
#include <vector>
#include "Renderer/FPBRMath.hpp"

TEST_SUITE("PBR - Energy Conservation Invariants") {

    TEST_CASE("Diffuse and Specular Fractions Satisfy kD + kS <= 1.0") {
        std::vector<float> metallicValues = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
        std::vector<float> cosThetaValues = {0.0f, 0.2f, 0.5f, 0.8f, 1.0f};
        glm::vec3 albedo(0.8f, 0.2f, 0.3f);

        for (float m : metallicValues) {
            glm::vec3 F0 = glm::mix(glm::vec3(0.04f), albedo, m);
            for (float cosT : cosThetaValues) {
                glm::vec3 F = Leon::FresnelSchlick(cosT, F0);
                glm::vec3 kS = F;
                glm::vec3 kD = (glm::vec3(1.0f) - kS) * (1.0f - m);

                // 1. Total energy fraction must be <= 1.0 on every color channel
                glm::vec3 total = kD + kS;
                CHECK(total.r <= 1.0001f);
                CHECK(total.g <= 1.0001f);
                CHECK(total.b <= 1.0001f);

                // 2. Validate using engine helper
                CHECK(Leon::ValidateEnergyConservation(kD, kS, m));

                // 3. For pure metals (m = 1.0), diffuse component must be strictly 0
                if (m >= 1.0f) {
                    CHECK(kD.r == doctest::Approx(0.0f).epsilon(1e-5f));
                    CHECK(kD.g == doctest::Approx(0.0f).epsilon(1e-5f));
                    CHECK(kD.b == doctest::Approx(0.0f).epsilon(1e-5f));
                }

                // 4. For pure dielectrics (m = 0.0), total fraction must equal 1.0 exactly
                if (m <= 0.0f) {
                    CHECK(total.r == doctest::Approx(1.0f).epsilon(1e-5f));
                    CHECK(total.g == doctest::Approx(1.0f).epsilon(1e-5f));
                    CHECK(total.b == doctest::Approx(1.0f).epsilon(1e-5f));
                }
            }
        }
    }
}
