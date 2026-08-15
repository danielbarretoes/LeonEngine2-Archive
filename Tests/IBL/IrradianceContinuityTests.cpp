#include <doctest/doctest.h>
#include "asset/HDRImporter.hpp"
#include "renderer/IBLMath.hpp"

TEST_SUITE("IBL - Irradiance Cubemap Spatial Continuity") {

    TEST_CASE("All 6 Faces x 32x32 Spatial Neighbor Outlier Ratio <= 1.25x (Zero Fireflies)") {
        const std::string hdrPath = "Projects/Sandbox/Content/HDR/AutumnField1k.lhdr";
        if (!std::filesystem::exists(hdrPath)) {
            MESSAGE("AutumnField1k.lhdr not found — skipping asset continuity test.");
            return;
        }

        Leon::FNativeHDRData nativeData;
        REQUIRE(nativeData.LoadFromFile(hdrPath));
        int width = nativeData.Header.Width;
        int height = nativeData.Header.Height;
        REQUIRE(width == 1024);
        REQUIRE(height == 512);

        Leon::FHDREquirectangularMipChain mipChain;
        mipChain.Build(nativeData.Pixels.data(), width, height);

        const int irradSize = 32;
        const uint32_t sampleCount = 512;
        const float saSample = (2.0f * Leon::PI) / static_cast<float>(sampleCount);
        const float saTexel = (4.0f * Leon::PI) / static_cast<float>(width * height);
        const float lod = std::max(0.5f * std::log2(saSample / saTexel) + 1.0f, 0.0f);

        std::vector<std::vector<float>> faceLuminances(6, std::vector<float>(irradSize * irradSize, 0.0f));

        for (int face = 0; face < 6; ++face) {
            for (int y = 0; y < irradSize; ++y) {
                float v = 2.0f * (static_cast<float>(y) + 0.5f) / static_cast<float>(irradSize) - 1.0f;
                for (int x = 0; x < irradSize; ++x) {
                    float u = 2.0f * (static_cast<float>(x) + 0.5f) / static_cast<float>(irradSize) - 1.0f;
                    glm::vec3 N = Leon::GetCubeDirection(face, u, v);

                    glm::vec3 irr(0.0f);
                    for (uint32_t i = 0; i < sampleCount; ++i) {
                        glm::vec2 xi = Leon::Hammersley(i, sampleCount);
                        glm::vec3 sVec = Leon::CosineSampleHemisphere(xi, N);
                        irr += mipChain.SampleLod(sVec, lod);
                    }
                    irr = Leon::PI * irr / static_cast<float>(sampleCount);
                    float lum = 0.2126f * irr.r + 0.7152f * irr.g + 0.0722f * irr.b;

                    faceLuminances[face][y * irradSize + x] = lum;
                }
            }
        }

        // Test spatial continuity & neighbor outlier ratios across all 6144 texels
        float maxRatio = 0.0f;
        float maxDelta = 0.0f;

        for (int face = 0; face < 6; ++face) {
            for (int y = 0; y < irradSize; ++y) {
                for (int x = 0; x < irradSize; ++x) {
                    float val = faceLuminances[face][y * irradSize + x];

                    // 1. Invariant: Valid finite non-negative values
                    CHECK(!std::isnan(val));
                    CHECK(!std::isinf(val));
                    CHECK(val >= 0.0f);

                    float sumNeigh = 0.0f;
                    int countNeigh = 0;
                    for (int dy = -1; dy <= 1; ++dy) {
                        for (int dx = -1; dx <= 1; ++dx) {
                            if (dx == 0 && dy == 0) continue;
                            int nx = x + dx;
                            int ny = y + dy;
                            if (nx >= 0 && nx < irradSize && ny >= 0 && ny < irradSize) {
                                sumNeigh += faceLuminances[face][ny * irradSize + nx];
                                countNeigh++;
                            }
                        }
                    }
                    if (countNeigh > 0) {
                        float meanNeigh = sumNeigh / static_cast<float>(countNeigh);
                        float ratio = val / std::max(meanNeigh, 0.001f);
                        float delta = std::abs(val - meanNeigh);
                        if (ratio > maxRatio) maxRatio = ratio;
                        if (delta > maxDelta) maxDelta = delta;

                        // Invariant: No isolated single-texel spikes (ratio must be < 1.25x)
                        CHECK(ratio <= 1.25f);
                    }
                }
            }
        }

        CHECK(maxRatio <= 1.25f);
    }
}
