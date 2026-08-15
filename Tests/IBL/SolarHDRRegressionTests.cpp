#include <doctest/doctest.h>
#include <stb_image.h>
#include "renderer/IBLMath.hpp"

TEST_SUITE("IBL - Solar HDR Regression Tests") {

    TEST_CASE("AutumnField1k.hdr Solar Irradiance Convergence & Bounded Discontinuity") {
        const std::string hdrPath = "Projects/Sandbox/Content/Assets/Hdr/AutumnField1k.hdr";
        if (!std::filesystem::exists(hdrPath)) {
            MESSAGE("AutumnField1k.hdr not found in working directory — skipping real asset test.");
            return;
        }

        int width = 0, height = 0, channels = 0;
        stbi_set_flip_vertically_on_load(0);
        float* hdrData = stbi_loadf(hdrPath.c_str(), &width, &height, &channels, 4);
        REQUIRE(hdrData != nullptr);
        REQUIRE(width == 1024);
        REQUIRE(height == 512);

        Leon::FHDREquirectangularMipChain mipChain;
        mipChain.Build(hdrData, width, height);
        stbi_image_free(hdrData);

        // Sun Direction in World Space from (615.5, 173.5)
        float uSun = (615.0f + 0.5f) / static_cast<float>(width);
        float vSun = (173.0f + 0.5f) / static_cast<float>(height);
        float phiSun = (uSun - 0.5f) * Leon::TWO_PI;
        float thetaSun = (vSun - 0.5f) * Leon::PI;
        glm::vec3 sunDir(
            std::cos(thetaSun) * std::cos(phiSun),
            std::sin(thetaSun),
            std::cos(thetaSun) * std::sin(phiSun)
        );
        sunDir = glm::normalize(sunDir);

        std::vector<std::pair<std::string, glm::vec3>> testDirections = {
            { "+X", glm::vec3(1.0f, 0.0f, 0.0f) },
            { "-X", glm::vec3(-1.0f, 0.0f, 0.0f) },
            { "+Y", glm::vec3(0.0f, 1.0f, 0.0f) },
            { "-Y", glm::vec3(0.0f, -1.0f, 0.0f) },
            { "+Z", glm::vec3(0.0f, 0.0f, 1.0f) },
            { "-Z", glm::vec3(0.0f, 0.0f, -1.0f) },
            { "Towards Sun", sunDir }
        };

        const std::vector<uint32_t> sampleCounts = {256, 512, 1024};

        for (const auto& [name, N] : testDirections) {
            float prevLum = 0.0f;
            for (uint32_t N_samples : sampleCounts) {
                float saSample = (2.0f * Leon::PI) / static_cast<float>(N_samples);
                float saTexel = (4.0f * Leon::PI) / static_cast<float>(width * height);
                float lod = std::max(0.5f * std::log2(saSample / saTexel) + 1.0f, 0.0f);

                glm::vec3 irr(0.0f);
                for (uint32_t i = 0; i < N_samples; ++i) {
                    glm::vec2 xi = Leon::Hammersley(i, N_samples);
                    glm::vec3 sVec = Leon::CosineSampleHemisphere(xi, N);
                    irr += mipChain.SampleLod(sVec, lod);
                }
                irr = Leon::PI * irr / static_cast<float>(N_samples);
                float lum = 0.2126f * irr.r + 0.7152f * irr.g + 0.0722f * irr.b;

                // 1. Invariant: Lum must be non-negative and finite
                CHECK(lum >= 0.0f);
                CHECK(!std::isnan(lum));
                CHECK(!std::isinf(lum));

                // 2. Invariant: Lum must NEVER explode to raw single-pixel spikes (> 25.0)
                CHECK(lum < 25.0f);

                // 3. Invariant: Convergence delta between 512 and 1024 samples must be < 1.0
                if (N_samples == 1024 && prevLum > 0.0f) {
                    CHECK(std::abs(lum - prevLum) < 1.0f);
                }
                prevLum = lum;
            }
        }
    }
}
