#include <doctest/doctest.h>
#include <cstring>
#include "Renderer/FIBLMath.hpp"
#include "Fixtures/GenerateFixtures.hpp"

TEST_SUITE("IBL - Algorithm Determinism Tests") {

    TEST_CASE("Repeatable 100% Deterministic Execution across Multiple Runs") {
        const int W = 32, H = 16;
        auto testData =
            Leon::TestFixtures::CreateHemisphereStepHDR(W, H, glm::vec3(2.0f, 1.0f, 0.5f), glm::vec3(0.1f, 0.2f, 0.3f));

        // 1. MipChain build determinism
        Leon::FHDREquirectangularMipChain chain1;
        chain1.Build(testData.data(), W, H);

        Leon::FHDREquirectangularMipChain chain2;
        chain2.Build(testData.data(), W, H);

        REQUIRE(chain1.Levels.size() == chain2.Levels.size());
        for (size_t lvl = 0; lvl < chain1.Levels.size(); ++lvl) {
            REQUIRE(chain1.Levels[lvl].Data.size() == chain2.Levels[lvl].Data.size());
            CHECK(std::memcmp(chain1.Levels[lvl].Data.data(), chain2.Levels[lvl].Data.data(),
                              chain1.Levels[lvl].Data.size() * sizeof(float)) == 0);
        }

        // 2. BRDF Integration Determinism
        for (float nv : {0.1f, 0.5f, 0.9f}) {
            for (float r : {0.1f, 0.5f, 0.9f}) {
                glm::vec2 brdf1 = Leon::IntegrateBRDF(nv, r, 256);
                glm::vec2 brdf2 = Leon::IntegrateBRDF(nv, r, 256);
                CHECK(brdf1.x == brdf2.x);
                CHECK(brdf1.y == brdf2.y);
            }
        }

        // 3. Irradiance Sampling Determinism
        glm::vec3 N = glm::normalize(glm::vec3(0.5f, 0.8f, -0.3f));
        glm::vec3 irr1(0.0f);
        glm::vec3 irr2(0.0f);
        for (uint32_t i = 0; i < 256; ++i) {
            glm::vec2 xi = Leon::Hammersley(i, 256);
            glm::vec3 sVec = Leon::CosineSampleHemisphere(xi, N);
            irr1 += chain1.SampleLod(sVec, 2.0f);
            irr2 += chain2.SampleLod(sVec, 2.0f);
        }
        CHECK(irr1.r == irr2.r);
        CHECK(irr1.g == irr2.g);
        CHECK(irr1.b == irr2.b);
    }
}
