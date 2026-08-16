#include <doctest/doctest.h>
#include <fstream>
#include "Renderer/FIBLMath.hpp"

TEST_SUITE("PBR - BRDF LUT Invariants") {

    TEST_CASE("Analytical Split-Sum BRDF LUT Invariants across (NdotV, Roughness)") {
        std::vector<float> testNdotV = {0.001f, 0.25f, 0.5f, 0.75f, 1.0f};
        std::vector<float> testRoughness = {0.001f, 0.25f, 0.5f, 0.75f, 1.0f};

        for (float r : testRoughness) {
            for (float nv : testNdotV) {
                glm::vec2 brdf = Leon::IntegrateBRDF(nv, r, 512);

                // 1. Invariants: Valid finite numbers
                CHECK(!std::isnan(brdf.x));
                CHECK(!std::isnan(brdf.y));
                CHECK(!std::isinf(brdf.x));
                CHECK(!std::isinf(brdf.y));

                // 2. Invariants: Range bounds [0, 1]
                CHECK(brdf.x >= 0.0f);
                CHECK(brdf.x <= 1.05f);
                CHECK(brdf.y >= 0.0f);
                CHECK(brdf.y <= 1.05f);

                // 3. Energy invariant: Total reflectance factor <= 1.0
                CHECK(brdf.x + brdf.y <= 1.05f);
            }
        }

        // Boundary check: At perpendicular view NdotV = 1.0, roughness = 0.0:
        // Scale term A should be ~1.0, Bias term B should be ~0.0
        glm::vec2 mirrorBRDF = Leon::IntegrateBRDF(1.0f, 0.001f, 1024);
        CHECK(mirrorBRDF.x == doctest::Approx(1.0f).epsilon(0.05f));
        CHECK(mirrorBRDF.y == doctest::Approx(0.0f).epsilon(0.05f));
    }

    TEST_CASE("Pre-baked BRDF_LUT.bin File Verification") {
        const std::string binPath = "Engine/Assets/Textures/BRDF_LUT.bin";
        if (!std::filesystem::exists(binPath)) {
            MESSAGE("BRDF_LUT.bin not found on disk — skipping file check.");
            return;
        }

        std::ifstream file(binPath, std::ios::binary);
        REQUIRE(file.is_open());

        file.seekg(0, std::ios::end);
        size_t fileSize = file.tellg();
        file.seekg(0, std::ios::beg);

        // Size check: 256x256x2 floats = 524,288 bytes (or 512x512x2 = 2,097,152)
        REQUIRE((fileSize == 524288 || fileSize == 2097152));

        std::vector<float> data(fileSize / sizeof(float));
        file.read(reinterpret_cast<char*>(data.data()), fileSize);

        for (float val : data) {
            CHECK(!std::isnan(val));
            CHECK(!std::isinf(val));
            CHECK(val >= 0.0f);
            CHECK(val <= 1.05f);
        }
    }
}
