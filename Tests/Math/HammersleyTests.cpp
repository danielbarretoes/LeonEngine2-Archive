#include <doctest/doctest.h>
#include "Renderer/FIBLMath.hpp"

TEST_SUITE("Math - Hammersley & Quasi-Monte Carlo") {

    TEST_CASE("Van der Corput Radical Inverse Bounds and Monotonicity") {
        for (uint32_t i = 0; i < 1024; ++i) {
            float vdc = Leon::RadicalInverse_VdC(i);
            CHECK(vdc >= 0.0f);
            CHECK(vdc < 1.0f);
        }
        // Known exact Van der Corput values:
        CHECK(Leon::RadicalInverse_VdC(0) == doctest::Approx(0.0f));
        CHECK(Leon::RadicalInverse_VdC(1) == doctest::Approx(0.5f));
        CHECK(Leon::RadicalInverse_VdC(2) == doctest::Approx(0.25f));
        CHECK(Leon::RadicalInverse_VdC(3) == doctest::Approx(0.75f));
        CHECK(Leon::RadicalInverse_VdC(4) == doctest::Approx(0.125f));
    }

    TEST_CASE("Hammersley 2D Point Distribution in [0, 1)^2") {
        const uint32_t N = 1024;
        for (uint32_t i = 0; i < N; ++i) {
            glm::vec2 xi = Leon::Hammersley(i, N);
            CHECK(xi.x >= 0.0f);
            CHECK(xi.x < 1.0f);
            CHECK(xi.y >= 0.0f);
            CHECK(xi.y < 1.0f);
            CHECK(xi.x == doctest::Approx(static_cast<float>(i) / static_cast<float>(N)));
        }
    }

    TEST_CASE("Cosine-Weighted Hemisphere Statistical Expectation E[cos(theta)] = 2/3") {
        const uint32_t N = 4096;
        glm::vec3 normal(0.0f, 1.0f, 0.0f);
        float sumCosTheta = 0.0f;

        for (uint32_t i = 0; i < N; ++i) {
            glm::vec2 xi = Leon::Hammersley(i, N);
            glm::vec3 sampleDir = Leon::CosineSampleHemisphere(xi, normal);

            // 1. Length must be unit 1.0
            CHECK(std::abs(glm::length(sampleDir) - 1.0f) < 1e-5f);

            // 2. Must lie on upper hemisphere (N . sampleDir >= 0)
            float cosTheta = glm::dot(normal, sampleDir);
            CHECK(cosTheta >= -1e-5f);

            sumCosTheta += std::max(0.0f, cosTheta);
        }

        float meanCosTheta = sumCosTheta / static_cast<float>(N);
        // Analytical expectation for cosine-weighted distribution is exactly 2/3 = 0.666667
        CHECK(meanCosTheta == doctest::Approx(2.0f / 3.0f).epsilon(0.015f));
    }
}
