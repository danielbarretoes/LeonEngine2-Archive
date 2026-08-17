#include <doctest/doctest.h>
#include "Assets/FHDRImporter.hpp"
#include <cmath>
#include <filesystem>
#include <vector>

TEST_SUITE("HDR - Decoding & Radiance Invariants") {

    TEST_CASE("DaySky1k.lhdr native binary decoding invariants") {
        const std::string hdrPath = "Projects/Sandbox/Content/HDR/DaySky1k.lhdr";
        REQUIRE(std::filesystem::exists(hdrPath));

        Leon::FNativeHDRData data;
        REQUIRE(data.LoadFromFile(hdrPath));
        REQUIRE(data.Header.Width > 0);
        REQUIRE(data.Header.Height > 0);
        const int width = data.Header.Width;
        const int height = data.Header.Height;
        REQUIRE(data.Pixels.size() == static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);

        const float* pixels = data.Pixels.data();
        float minLum = 999999.0f;
        float maxLum = -1.0f;
        for (int i = 0; i < width * height; ++i) {
            const float r = pixels[i * 4 + 0];
            const float g = pixels[i * 4 + 1];
            const float b = pixels[i * 4 + 2];

            CHECK(!std::isnan(r));
            CHECK(!std::isnan(g));
            CHECK(!std::isnan(b));
            CHECK(!std::isinf(r));
            CHECK(!std::isinf(g));
            CHECK(!std::isinf(b));
            CHECK(r >= 0.0f);
            CHECK(g >= 0.0f);
            CHECK(b >= 0.0f);

            const float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
            if (lum < minLum)
                minLum = lum;
            if (lum > maxLum)
                maxLum = lum;
        }

        CHECK(minLum >= 0.0f);
        CHECK(maxLum > minLum);
        CHECK(maxLum > 1.0f);
    }
}
