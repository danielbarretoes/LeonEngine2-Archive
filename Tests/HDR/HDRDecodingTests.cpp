#include <doctest/doctest.h>
#include <stb_image.h>
#include <filesystem>
#include <vector>
#include <cmath>

TEST_SUITE("HDR - Decoding & Radiance Invariants") {

    TEST_CASE("AutumnField1k.hdr RGBE 32-bit Float Decoding & Solar Fixture") {
        const std::string hdrPath = "Projects/Sandbox/Content/Assets/Hdr/AutumnField1k.hdr";
        if (!std::filesystem::exists(hdrPath)) {
            MESSAGE("AutumnField1k.hdr not found — skipping file load test.");
            return;
        }

        int width = 0, height = 0, channels = 0;
        stbi_set_flip_vertically_on_load(0);
        float* data = stbi_loadf(hdrPath.c_str(), &width, &height, &channels, 4);
        REQUIRE(data != nullptr);
        REQUIRE(width == 1024);
        REQUIRE(height == 512);

        // 1. Invariant: All pixels must be non-negative and finite
        float minLum = 999999.0f;
        float maxLum = -1.0f;
        for (int i = 0; i < width * height; ++i) {
            float r = data[i * 4 + 0];
            float g = data[i * 4 + 1];
            float b = data[i * 4 + 2];

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

        // 2. Exact Solar Pixel Fixture check at (x=615, y=173)
        size_t sunIdx = (173 * width + 615) * 4;
        float sunR = data[sunIdx + 0];
        float sunG = data[sunIdx + 1];
        float sunB = data[sunIdx + 2];
        float sunLum = 0.2126f * sunR + 0.7152f * sunG + 0.0722f * sunB;

        CHECK(sunLum == doctest::Approx(114033.87f).epsilon(0.01f));

        stbi_image_free(data);
    }
}
