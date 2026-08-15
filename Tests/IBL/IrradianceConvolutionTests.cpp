#include <doctest/doctest.h>
#include "renderer/IBLMath.hpp"
#include "Fixtures/GenerateFixtures.hpp"

TEST_SUITE("IBL - Irradiance Convolution Invariants") {

    TEST_CASE("Constant Environment L = 1.0 Produces Exactly E = PI") {
        const int W = 64, H = 32;
        auto constWhiteData = Leon::TestFixtures::CreateConstantHDR(W, H, glm::vec3(1.0f));

        Leon::FHDREquirectangularMipChain mipChain;
        mipChain.Build(constWhiteData.data(), W, H);

        const uint32_t sampleCount = 512;
        const float saSample = (2.0f * Leon::PI) / static_cast<float>(sampleCount);
        const float saTexel = (4.0f * Leon::PI) / static_cast<float>(W * H);
        const float lod = std::max(0.5f * std::log2(saSample / saTexel) + 1.0f, 0.0f);

        std::vector<glm::vec3> testNormals = {
            glm::vec3(1.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, 1.0f),
            glm::normalize(glm::vec3(1.0f, 1.0f, 1.0f)),
            glm::normalize(glm::vec3(-0.5f, -0.5f, 0.707f))
        };

        for (const auto& N : testNormals) {
            glm::vec3 irradiance(0.0f);
            for (uint32_t i = 0; i < sampleCount; ++i) {
                glm::vec2 xi = Leon::Hammersley(i, sampleCount);
                glm::vec3 sampleDir = Leon::CosineSampleHemisphere(xi, N);
                irradiance += mipChain.SampleLod(sampleDir, lod);
            }
            irradiance = Leon::PI * irradiance / static_cast<float>(sampleCount);

            // Integral of cos(theta) over hemisphere is exactly PI
            CHECK(irradiance.r == doctest::Approx(Leon::PI).epsilon(0.01f));
            CHECK(irradiance.g == doctest::Approx(Leon::PI).epsilon(0.01f));
            CHECK(irradiance.b == doctest::Approx(Leon::PI).epsilon(0.01f));
        }
    }

    TEST_CASE("Black Environment L = 0.0 Produces Zero Irradiance") {
        const int W = 32, H = 16;
        auto blackData = Leon::TestFixtures::CreateConstantHDR(W, H, glm::vec3(0.0f));

        Leon::FHDREquirectangularMipChain mipChain;
        mipChain.Build(blackData.data(), W, H);

        glm::vec3 N(0.0f, 1.0f, 0.0f);
        glm::vec3 irradiance(0.0f);
        for (uint32_t i = 0; i < 256; ++i) {
            glm::vec2 xi = Leon::Hammersley(i, 256);
            glm::vec3 sampleDir = Leon::CosineSampleHemisphere(xi, N);
            irradiance += mipChain.SampleLod(sampleDir, 0.0f);
        }
        irradiance = Leon::PI * irradiance / 256.0f;

        CHECK(irradiance.r == doctest::Approx(0.0f).epsilon(1e-5f));
        CHECK(irradiance.g == doctest::Approx(0.0f).epsilon(1e-5f));
        CHECK(irradiance.b == doctest::Approx(0.0f).epsilon(1e-5f));
    }

    TEST_CASE("RGB Channel Isolation in Irradiance Convolution") {
        const int W = 32, H = 16;
        std::vector<glm::vec3> channelColors = {
            glm::vec3(1.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, 1.0f)
        };

        for (size_t c = 0; c < 3; ++c) {
            auto channelData = Leon::TestFixtures::CreateConstantHDR(W, H, channelColors[c]);
            Leon::FHDREquirectangularMipChain mipChain;
            mipChain.Build(channelData.data(), W, H);

            glm::vec3 N(1.0f, 0.0f, 0.0f);
            glm::vec3 irradiance(0.0f);
            for (uint32_t i = 0; i < 256; ++i) {
                glm::vec2 xi = Leon::Hammersley(i, 256);
                glm::vec3 sampleDir = Leon::CosineSampleHemisphere(xi, N);
                irradiance += mipChain.SampleLod(sampleDir, 0.0f);
            }
            irradiance = Leon::PI * irradiance / 256.0f;

            if (c == 0) {
                CHECK(irradiance.r == doctest::Approx(Leon::PI).epsilon(0.01f));
                CHECK(irradiance.g == doctest::Approx(0.0f).epsilon(1e-5f));
                CHECK(irradiance.b == doctest::Approx(0.0f).epsilon(1e-5f));
            } else if (c == 1) {
                CHECK(irradiance.r == doctest::Approx(0.0f).epsilon(1e-5f));
                CHECK(irradiance.g == doctest::Approx(Leon::PI).epsilon(0.01f));
                CHECK(irradiance.b == doctest::Approx(0.0f).epsilon(1e-5f));
            } else {
                CHECK(irradiance.r == doctest::Approx(0.0f).epsilon(1e-5f));
                CHECK(irradiance.g == doctest::Approx(0.0f).epsilon(1e-5f));
                CHECK(irradiance.b == doctest::Approx(Leon::PI).epsilon(0.01f));
            }
        }
    }

    TEST_CASE("Hemisphere Step Function: Up (+Y) = PI, Down (-Y) = 0, Horizon (+X) = PI/2") {
        const int W = 64, H = 32;
        auto stepData = Leon::TestFixtures::CreateHemisphereStepHDR(W, H, glm::vec3(1.0f), glm::vec3(0.0f));

        Leon::FHDREquirectangularMipChain mipChain;
        mipChain.Build(stepData.data(), W, H);

        const uint32_t N_samples = 1024;
        auto computeIrrad = [&](glm::vec3 N) -> float {
            glm::vec3 irr(0.0f);
            for (uint32_t i = 0; i < N_samples; ++i) {
                glm::vec2 xi = Leon::Hammersley(i, N_samples);
                glm::vec3 sVec = Leon::CosineSampleHemisphere(xi, N);
                irr += mipChain.SampleLevel(0, sVec);
            }
            return (Leon::PI * irr / static_cast<float>(N_samples)).r;
        };

        float upVal = computeIrrad(glm::vec3(0.0f, 1.0f, 0.0f));
        float downVal = computeIrrad(glm::vec3(0.0f, -1.0f, 0.0f));
        float horizonVal = computeIrrad(glm::vec3(1.0f, 0.0f, 0.0f));

        CHECK(upVal == doctest::Approx(Leon::PI).epsilon(0.05f));
        CHECK(downVal < 0.05f);
        CHECK(horizonVal == doctest::Approx(Leon::PI * 0.5f).epsilon(0.15f));
    }
}
