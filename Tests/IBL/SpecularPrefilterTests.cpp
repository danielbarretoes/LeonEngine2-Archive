#include <doctest/doctest.h>
#include "asset/HDRImporter.hpp"
#include "renderer/IBLMath.hpp"

TEST_SUITE("IBL - Specular Prefilter Regression Tests") {

    TEST_CASE("Specular Prefilter Mip Chain Smooth Roughness Dispersion (No Fireflies)") {
        const std::string hdrPath = "Projects/Sandbox/Content/HDR/AutumnField1k.lhdr";
        if (!std::filesystem::exists(hdrPath)) {
            MESSAGE("AutumnField1k.lhdr not found — skipping prefilter test.");
            return;
        }

        Leon::FNativeHDRData nativeData;
        REQUIRE(nativeData.LoadFromFile(hdrPath));
        int width = nativeData.Header.Width;
        int height = nativeData.Header.Height;

        Leon::FHDREquirectangularMipChain mipChain;
        mipChain.Build(nativeData.Pixels.data(), width, height);

        const uint32_t numMips = 5;
        const uint32_t baseSize = 128;
        const uint32_t sampleCount = 256;
        const float saTexel = (4.0f * Leon::PI) / static_cast<float>(width * height);

        // Directions to test: +X, +Z, Towards Sun
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

        std::vector<glm::vec3> testDirections = {
            glm::vec3(1.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, 1.0f),
            sunDir
        };

        for (const auto& R : testDirections) {
            glm::vec3 N = R;
            glm::vec3 V = R;
            float prevSunLum = 999999.0f;

            for (uint32_t mip = 0; mip < numMips; ++mip) {
                float roughness = static_cast<float>(mip) / static_cast<float>(numMips - 1);
                glm::vec3 prefilteredColor(0.0f);
                float totalWeight = 0.0f;

                for (uint32_t i = 0; i < sampleCount; ++i) {
                    glm::vec2 Xi = Leon::Hammersley(i, sampleCount);
                    glm::vec3 H = Leon::ImportanceSampleGGX(Xi, N, roughness);
                    glm::vec3 L = glm::normalize(2.0f * glm::dot(V, H) * H - V);

                    float NdotL = std::max(glm::dot(N, L), 0.0f);
                    if (NdotL > 0.0f) {
                        float NdotH = std::max(glm::dot(N, H), 0.0f);
                        float HdotV = std::max(glm::dot(H, V), 0.0f);

                        float D = (roughness * roughness * roughness * roughness) /
                                  (Leon::PI * std::pow(NdotH * NdotH * (roughness * roughness * roughness * roughness - 1.0f) + 1.0f, 2.0f));
                        float pdf = (D * NdotH / (4.0f * HdotV)) + 0.0001f;

                        float saSample = 1.0f / (static_cast<float>(sampleCount) * pdf + 0.0001f);
                        float mipLevel = roughness == 0.0f ? 0.0f : std::max(0.5f * std::log2(saSample / saTexel) + 1.0f, 0.0f);

                        prefilteredColor += mipChain.SampleLod(L, mipLevel) * NdotL;
                        totalWeight += NdotL;
                    }
                }
                prefilteredColor = prefilteredColor / std::max(totalWeight, 0.0001f);

                // 1. Invariant: Valid finite non-negative values
                CHECK(!std::isnan(prefilteredColor.r));
                CHECK(!std::isnan(prefilteredColor.g));
                CHECK(!std::isnan(prefilteredColor.b));
                CHECK(!std::isinf(prefilteredColor.r));
                CHECK(prefilteredColor.r >= 0.0f);

                float lum = 0.2126f * prefilteredColor.r + 0.7152f * prefilteredColor.g + 0.0722f * prefilteredColor.b;

                // 2. Invariant: Towards sun, higher roughness should spread the peak (luminance must decrease or stay smooth)
                if (R == sunDir) {
                    CHECK(lum <= prevSunLum + 50.0f); // Allow slight numerical tolerance
                    prevSunLum = lum;
                }
            }
        }
    }
}
