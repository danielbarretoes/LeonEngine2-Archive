#include <doctest/doctest.h>
#include "renderer/IBLMath.hpp"
#include "Fixtures/GenerateFixtures.hpp"

TEST_SUITE("HDR - Mipmap Pyramid & 360 Wrap Invariants") {

    TEST_CASE("11-Level Mipmap Pyramid Downsampling Down to 1x1") {
        const int W = 1024, H = 512;
        auto hdrData = Leon::TestFixtures::CreateConstantHDR(W, H, glm::vec3(1.5f, 2.0f, 0.5f));

        Leon::FHDREquirectangularMipChain mipChain;
        mipChain.Build(hdrData.data(), W, H);

        REQUIRE(mipChain.Levels.size() == 11);

        int expectedW = W;
        int expectedH = H;
        for (size_t lvl = 0; lvl < mipChain.Levels.size(); ++lvl) {
            const auto& mip = mipChain.Levels[lvl];
            CHECK(mip.Width == expectedW);
            CHECK(mip.Height == expectedH);
            CHECK(mip.Data.size() == static_cast<size_t>(expectedW) * expectedH * 4);

            for (size_t i = 0; i < mip.Data.size(); i += 4) {
                CHECK(mip.Data[i + 0] == doctest::Approx(1.5f));
                CHECK(mip.Data[i + 1] == doctest::Approx(2.0f));
                CHECK(mip.Data[i + 2] == doctest::Approx(0.5f));
                CHECK(mip.Data[i + 3] == doctest::Approx(1.0f));
            }

            expectedW = std::max(1, expectedW / 2);
            expectedH = std::max(1, expectedH / 2);
        }

        // The final level must be exactly 1x1
        CHECK(mipChain.Levels.back().Width == 1);
        CHECK(mipChain.Levels.back().Height == 1);
    }

    TEST_CASE("360 Equirectangular Horizontal Seam Continuity (u = 0.0 vs u = 1.0)") {
        const int W = 128, H = 64;
        auto stepData = Leon::TestFixtures::CreateHemisphereStepHDR(W, H, glm::vec3(3.0f, 1.0f, 0.5f), glm::vec3(0.2f, 0.4f, 0.8f));

        Leon::FHDREquirectangularMipChain mipChain;
        mipChain.Build(stepData.data(), W, H);

        // Direction at phi = -pi (u = 0.0) vs phi = +pi (u = 1.0)
        glm::vec3 dirWest(-1.0f, 0.0f, -0.0001f);
        glm::vec3 dirEast(-1.0f, 0.0f, 0.0001f);

        for (float lod = 0.0f; lod <= 5.0f; lod += 1.0f) {
            glm::vec3 sampleW = mipChain.SampleLod(dirWest, lod);
            glm::vec3 sampleE = mipChain.SampleLod(dirEast, lod);

            CHECK(sampleW.r == doctest::Approx(sampleE.r).epsilon(0.01f));
            CHECK(sampleW.g == doctest::Approx(sampleE.g).epsilon(0.01f));
            CHECK(sampleW.b == doctest::Approx(sampleE.b).epsilon(0.01f));
        }
    }
}
