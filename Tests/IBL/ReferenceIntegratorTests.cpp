#include <doctest/doctest.h>
#include "Renderer/FIBLMath.hpp"
#include "Fixtures/GenerateFixtures.hpp"

namespace {
    // Independent high-precision numerical Riemann hemisphere integrator
    glm::vec3 IntegrateHemisphereRiemann(const Leon::FHDREquirectangularMipChain& mipChain, glm::vec3 N, int numTheta = 180, int numPhi = 360) {
        glm::vec3 up = (std::abs(N.z) < 0.999f) ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
        glm::vec3 T = glm::normalize(glm::cross(up, N));
        glm::vec3 B = glm::normalize(glm::cross(N, T));

        const float dTheta = (Leon::PI * 0.5f) / static_cast<float>(numTheta);
        const float dPhi = Leon::TWO_PI / static_cast<float>(numPhi);

        glm::vec3 totalIrradiance(0.0f);

        for (int t = 0; t < numTheta; ++t) {
            float theta = (static_cast<float>(t) + 0.5f) * dTheta;
            float cosTheta = std::cos(theta);
            float sinTheta = std::sin(theta);

            for (int p = 0; p < numPhi; ++p) {
                float phi = (static_cast<float>(p) + 0.5f) * dPhi;

                glm::vec3 localDir(sinTheta * std::cos(phi), sinTheta * std::sin(phi), cosTheta);
                glm::vec3 worldDir = glm::normalize(T * localDir.x + B * localDir.y + N * localDir.z);

                glm::vec3 radiance = mipChain.SampleLevel(0, worldDir);
                // Differential solid angle * cos(theta): sin(theta) * cos(theta) * dTheta * dPhi
                totalIrradiance += radiance * (sinTheta * cosTheta * dTheta * dPhi);
            }
        }
        return totalIrradiance;
    }
}

TEST_SUITE("IBL - Reference Integrator Comparison") {

    TEST_CASE("Engine Cosine-Weighted Hammersley vs High-Resolution Riemann Integrator") {
        const int W = 64, H = 32;
        auto smoothSkyData = Leon::TestFixtures::CreateSmoothGradientHDR(
            W, H,
            glm::vec3(2.5f, 1.2f, 0.8f), // Zenith
            glm::vec3(1.0f, 0.8f, 0.6f), // Horizon
            glm::vec3(0.1f, 0.2f, 0.3f)  // Ground
        );

        Leon::FHDREquirectangularMipChain mipChain;
        mipChain.Build(smoothSkyData.data(), W, H);

        std::vector<glm::vec3> testNormals = {
            glm::vec3(0.0f, 1.0f, 0.0f),
            glm::vec3(0.0f, -1.0f, 0.0f),
            glm::vec3(1.0f, 0.0f, 0.0f),
            glm::normalize(glm::vec3(0.5f, 0.8f, 0.3f))
        };

        const uint32_t N_samples = 512;
        const float saSample = (2.0f * Leon::PI) / static_cast<float>(N_samples);
        const float saTexel = (4.0f * Leon::PI) / static_cast<float>(W * H);
        const float lod = std::max(0.5f * std::log2(saSample / saTexel) + 1.0f, 0.0f);

        for (const auto& N : testNormals) {
            glm::vec3 ref = IntegrateHemisphereRiemann(mipChain, N, 100, 200);

            glm::vec3 engine(0.0f);
            for (uint32_t i = 0; i < N_samples; ++i) {
                glm::vec2 xi = Leon::Hammersley(i, N_samples);
                glm::vec3 sVec = Leon::CosineSampleHemisphere(xi, N);
                engine += mipChain.SampleLod(sVec, lod);
            }
            engine = Leon::PI * engine / static_cast<float>(N_samples);

            // Relative error between engine and reference ground truth must be < 8%
            for (int c = 0; c < 3; ++c) {
                float relErr = std::abs(engine[c] - ref[c]) / std::max(ref[c], 0.01f);
                CHECK(relErr < 0.08f);
            }
        }
    }
}
