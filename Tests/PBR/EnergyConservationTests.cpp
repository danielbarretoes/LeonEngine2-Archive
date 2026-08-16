#include <doctest/doctest.h>
#include <cmath>
#include <vector>
#include "Renderer/FPBRMath.hpp"
#include "Renderer/FIBLMath.hpp"

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

    TEST_CASE("Cosine-weighted hemisphere integral of Cook-Torrance stays <= 1") {
        glm::vec3 N(0.0f, 0.0f, 1.0f);
        glm::vec3 V(0.0f, 0.0f, 1.0f);
        glm::vec3 albedo(1.0f);
        const uint32_t sampleCount = 2048;

        auto integrate = [&](float metallic, float roughness) {
            double acc = 0.0;
            uint32_t used = 0;
            for (uint32_t i = 0; i < sampleCount; ++i) {
                glm::vec2 xi = Leon::Hammersley(i, sampleCount);
                glm::vec3 L = Leon::CosineSampleHemisphere(xi, N);
                float NdotL = std::max(glm::dot(N, L), 0.0f);
                float pdf = Leon::CosineHemispherePDF(NdotL);
                if (pdf < 1e-8f)
                    continue;
                glm::vec3 lo = Leon::EvaluateCookTorrance(N, V, L, albedo, metallic, roughness, glm::vec3(1.0f));
                acc += static_cast<double>(lo.r / pdf);
                ++used;
            }
            return acc / static_cast<double>(used);
        };

        CHECK(integrate(0.0f, 1.0f) <= 1.08);
        CHECK(integrate(0.0f, 0.5f) <= 1.08);
        CHECK(integrate(1.0f, 0.5f) <= 1.08);
    }
}
