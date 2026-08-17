#include <doctest/doctest.h>
#include "Assets/FHDRImporter.hpp"
#include "Renderer/FIBLMath.hpp"

#include <cmath>
#include <algorithm>
#include <filesystem>
#include <utility>
#include <vector>

TEST_SUITE("IBL - Solar HDR Regression Tests") {

    TEST_CASE("DaySky1k.lhdr irradiance convergence stays bounded") {
        const std::string hdrPath = "Projects/Sandbox/Content/HDR/DaySky1k.lhdr";
        REQUIRE(std::filesystem::exists(hdrPath));

        Leon::FNativeHDRData nativeData;
        REQUIRE(nativeData.LoadFromFile(hdrPath));
        const int width = nativeData.Header.Width;
        const int height = nativeData.Header.Height;
        REQUIRE(width > 0);
        REQUIRE(height > 0);

        Leon::FHDREquirectangularMipChain mipChain;
        mipChain.Build(nativeData.Pixels.data(), width, height);

        const std::vector<std::pair<std::string, glm::vec3>> testDirections = {
            {"+X", glm::vec3(1.0f, 0.0f, 0.0f)}, {"-X", glm::vec3(-1.0f, 0.0f, 0.0f)},
            {"+Y", glm::vec3(0.0f, 1.0f, 0.0f)}, {"-Y", glm::vec3(0.0f, -1.0f, 0.0f)},
            {"+Z", glm::vec3(0.0f, 0.0f, 1.0f)}, {"-Z", glm::vec3(0.0f, 0.0f, -1.0f)},
        };

        const std::vector<uint32_t> sampleCounts = {256, 512, 1024};

        for (const auto& [name, N] : testDirections) {
            float prevLum = 0.0f;
            for (uint32_t N_samples : sampleCounts) {
                const float saSample = (2.0f * Leon::PI) / static_cast<float>(N_samples);
                const float saTexel = (4.0f * Leon::PI) / static_cast<float>(width * height);
                const float lod = std::max(0.5f * std::log2(saSample / saTexel) + 1.0f, 0.0f);

                glm::vec3 irr(0.0f);
                for (uint32_t i = 0; i < N_samples; ++i) {
                    glm::vec2 xi = Leon::Hammersley(i, N_samples);
                    glm::vec3 sVec = Leon::CosineSampleHemisphere(xi, N);
                    irr += mipChain.SampleLod(sVec, lod);
                }
                irr = Leon::PI * irr / static_cast<float>(N_samples);
                const float lum = 0.2126f * irr.r + 0.7152f * irr.g + 0.0722f * irr.b;

                CHECK(lum >= 0.0f);
                CHECK(!std::isnan(lum));
                CHECK(!std::isinf(lum));
                CHECK(lum < 25.0f);

                if (N_samples == 1024 && prevLum > 0.0f) {
                    CHECK(std::abs(lum - prevLum) < 1.0f);
                }
                prevLum = lum;
            }
        }
    }
}
