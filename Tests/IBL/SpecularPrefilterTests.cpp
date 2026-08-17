#include <doctest/doctest.h>
#include "Assets/FHDRImporter.hpp"
#include "Renderer/FIBLMath.hpp"

#include <cmath>
#include <filesystem>
#include <vector>

TEST_SUITE("IBL - Specular Prefilter Regression Tests") {

    TEST_CASE("Specular Prefilter Mip Chain Smooth Roughness Dispersion (No Fireflies)") {
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

        const uint32_t numMips = 5;
        const uint32_t sampleCount = 256;
        const float saTexel = (4.0f * Leon::PI) / static_cast<float>(width * height);

        const std::vector<glm::vec3> testDirections = {
            glm::vec3(1.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, 1.0f),
        };

        for (const auto& R : testDirections) {
            const glm::vec3 N = R;
            const glm::vec3 V = R;
            float prevLum = 999999.0f;

            for (uint32_t mip = 0; mip < numMips; ++mip) {
                const float roughness = static_cast<float>(mip) / static_cast<float>(numMips - 1);
                glm::vec3 prefilteredColor(0.0f);
                float totalWeight = 0.0f;

                for (uint32_t i = 0; i < sampleCount; ++i) {
                    glm::vec2 Xi = Leon::Hammersley(i, sampleCount);
                    glm::vec3 H = Leon::ImportanceSampleGGX(Xi, N, roughness);
                    glm::vec3 L = glm::normalize(2.0f * glm::dot(V, H) * H - V);

                    const float NdotL = std::max(glm::dot(N, L), 0.0f);
                    if (NdotL > 0.0f) {
                        const float NdotH = std::max(glm::dot(N, H), 0.0f);
                        const float HdotV = std::max(glm::dot(H, V), 0.0f);

                        const float D =
                            (roughness * roughness * roughness * roughness) /
                            (Leon::PI *
                             std::pow(NdotH * NdotH * (roughness * roughness * roughness * roughness - 1.0f) + 1.0f,
                                      2.0f));
                        const float pdf = (D * NdotH / (4.0f * HdotV)) + 0.0001f;

                        const float saSample = 1.0f / (static_cast<float>(sampleCount) * pdf + 0.0001f);
                        const float mipLevel =
                            roughness == 0.0f ? 0.0f : std::max(0.5f * std::log2(saSample / saTexel) + 1.0f, 0.0f);

                        prefilteredColor += mipChain.SampleLod(L, mipLevel) * NdotL;
                        totalWeight += NdotL;
                    }
                }
                prefilteredColor = prefilteredColor / std::max(totalWeight, 0.0001f);

                CHECK(!std::isnan(prefilteredColor.r));
                CHECK(!std::isnan(prefilteredColor.g));
                CHECK(!std::isnan(prefilteredColor.b));
                CHECK(!std::isinf(prefilteredColor.r));
                CHECK(prefilteredColor.r >= 0.0f);

                const float lum =
                    0.2126f * prefilteredColor.r + 0.7152f * prefilteredColor.g + 0.0722f * prefilteredColor.b;
                CHECK(lum <= prevLum + 50.0f);
                prevLum = lum;
            }
        }
    }
}
