#include <doctest/doctest.h>
#include "Assets/FHDRImporter.hpp"
#include <filesystem>
#include <vector>
#include <cmath>

TEST_SUITE("HDR - Decoding & Radiance Invariants") {

    TEST_CASE("AutumnField1k.lhdr Native Binary Decoding & Solar Fixture") {
        const std::string hdrPath = "Projects/Sandbox/Content/HDR/AutumnField1k.lhdr";
        if (!std::filesystem::exists(hdrPath)) {
            MESSAGE("AutumnField1k.lhdr not found — skipping file load test.");
            return;
        }

        Leon::FNativeHDRData data;
        REQUIRE(data.LoadFromFile(hdrPath));
        REQUIRE(data.Header.Width == 1024);
        REQUIRE(data.Header.Height == 512);
        REQUIRE(data.Pixels.size() == 1024 * 512 * 4);

        int width = 1024;
        int height = 512;
        const float* pixels = data.Pixels.data();

        // 1. Invariant: All pixels must be non-negative and finite
        float minLum = 999999.0f;
        float maxLum = -1.0f;
        for (int i = 0; i < width * height; ++i) {
            float r = pixels[i * 4 + 0];
            float g = pixels[i * 4 + 1];
            float b = pixels[i * 4 + 2];

            CHECK(!std::isnan(r));
            CHECK(!std::isnan(g));
            CHECK(!std::isnan(b));
            CHECK(!std::isinf(r));
            CHECK(!std::isinf(g));
            CHECK(!std::isinf(b));
            CHECK(r >= 0.0f);
            CHECK(g >= 0.0f);
            CHECK(b >= 0.0f);

            float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
            if (lum < minLum) minLum = lum;
            if (lum > maxLum) maxLum = lum;
        }

        CHECK(minLum > 0.01f);
        CHECK(maxLum > 100000.0f); // Solar core

        // 2. Exact Solar Pixel Fixture check at (x=615, y=338 in flipped texture space)
        size_t sunIdx = (338 * width + 615) * 4;
        float sunR = pixels[sunIdx + 0];
        float sunG = pixels[sunIdx + 1];
        float sunB = pixels[sunIdx + 2];
        float sunLum = 0.2126f * sunR + 0.7152f * sunG + 0.0722f * sunB;

        CHECK(sunLum == doctest::Approx(114033.87f).epsilon(0.01f));
    }
}
