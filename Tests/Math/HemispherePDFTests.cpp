#include <doctest/doctest.h>
#include "renderer/IBLMath.hpp"

TEST_SUITE("Math - Hemisphere PDF & Integration Invariants") {

    TEST_CASE("Cosine-Weighted PDF Non-Negativity and Analytical Normalization") {
        const float eps = 1e-6f;

        // 1. Non-negativity test
        for (float theta = 0.0f; theta <= Leon::PI * 0.5f; theta += 0.05f) {
            float cosTheta = std::cos(theta);
            float pdf = Leon::CosineHemispherePDF(cosTheta);
            CHECK(pdf >= 0.0f);
            CHECK(pdf <= 1.0f / Leon::PI + eps);
        }

        // 2. Numerical Riemann Integration of PDF over the hemisphere: Integral(PDF dOmega) == 1.0
        const int stepsTheta = 500;
        const int stepsPhi = 1000;
        const float dTheta = (Leon::PI * 0.5f) / static_cast<float>(stepsTheta);
        const float dPhi = Leon::TWO_PI / static_cast<float>(stepsPhi);

        double totalIntegral = 0.0;

        for (int t = 0; t < stepsTheta; ++t) {
            float theta = (static_cast<float>(t) + 0.5f) * dTheta;
            float cosTheta = std::cos(theta);
            float sinTheta = std::sin(theta);
            float pdf = Leon::CosineHemispherePDF(cosTheta);

            // Differential solid angle dOmega = sin(theta) * dTheta * dPhi
            double dOmega = static_cast<double>(sinTheta) * dTheta * dPhi;
            totalIntegral += static_cast<double>(pdf) * dOmega;
        }
        totalIntegral *= stepsPhi; // Symmetry across phi

        CHECK(totalIntegral == doctest::Approx(1.0).epsilon(0.001));
    }
}
