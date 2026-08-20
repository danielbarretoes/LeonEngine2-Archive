#include <doctest/doctest.h>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <vector>
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

    TEST_CASE("BRDF LUT disk header is v2 and 24 bytes") {
        CHECK(sizeof(Leon::FBRDFLUTDiskHeader) == 24);
        Leon::FBRDFLUTDiskHeader header;
        CHECK(header.Version == 2);
        CHECK(header.SampleCount == Leon::kBRDFLUTSampleCount);
    }

    TEST_CASE("Pre-baked BRDF_LUT.bin File Verification") {
        const std::string binPath = "Engine/Resources/Textures/BRDF_LUT.bin";
        REQUIRE(std::filesystem::exists(binPath));

        std::ifstream file(binPath, std::ios::binary);
        REQUIRE(file.is_open());

        file.seekg(0, std::ios::end);
        const size_t fileSize = static_cast<size_t>(file.tellg());
        file.seekg(0, std::ios::beg);

        Leon::FBRDFLUTDiskHeader header{};
        file.read(reinterpret_cast<char*>(&header), sizeof(header));
        REQUIRE(file);
        REQUIRE(std::string(header.Magic, 8) == "LEONBRDF");
        REQUIRE(header.Version == 2);
        const bool sizeOk = header.Size == 256 || header.Size == 512;
        REQUIRE(sizeOk);
        REQUIRE(header.SampleCount > 0);

        const size_t payloadBytes =
            static_cast<size_t>(header.Size) * static_cast<size_t>(header.Size) * 2u * sizeof(float);
        REQUIRE(fileSize == sizeof(Leon::FBRDFLUTDiskHeader) + payloadBytes);

        std::vector<float> data(payloadBytes / sizeof(float));
        file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(payloadBytes));
        REQUIRE(file.gcount() == static_cast<std::streamsize>(payloadBytes));

        for (float val : data) {
            CHECK(!std::isnan(val));
            CHECK(!std::isinf(val));
            CHECK(val >= 0.0f);
            CHECK(val <= 1.05f);
        }
    }
}
