#include <doctest/doctest.h>
#include "Assets/FHDRImporter.hpp"
#include "Renderer/FIBLMath.hpp"
#include "Core/Base.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <vector>
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;
using namespace Leon;

TEST_SUITE("HDR Asset Pipeline Tests") {

    TEST_CASE("HDR - Procedural Atmospheric HDR Synthesis") {
        FNativeHDRData data;
        glm::vec3 zenith(0.08f, 0.28f, 0.52f);
        glm::vec3 horizon(0.22f, 0.55f, 0.70f);
        glm::vec3 ground(0.02f, 0.08f, 0.16f);
        glm::vec3 sunColor(1.0f, 0.95f, 0.85f);
        glm::vec3 sunDir(0.25f, 0.85f, 0.45f);

        bool ok = FHDRImporter::CreateAtmosphericHDR(64, 32, zenith, horizon, ground, sunColor, sunDir, 5.0f, data);
        CHECK(ok);
        CHECK(data.Header.Magic == LHDR_MAGIC);
        CHECK(data.Header.Version == LHDR_VERSION);
        CHECK(data.Header.Width == 64);
        CHECK(data.Header.Height == 32);
        CHECK(data.Header.Channels == 4);
        CHECK(data.Pixels.size() == 64 * 32 * 4);

        float maxVal = 0.0f;
        for (float val : data.Pixels) {
            maxVal = std::max(maxVal, val);
        }
        CHECK(maxVal > 1.0f);
    }

    TEST_CASE("HDR - Native .lhdr Binary Save & Load Roundtrip") {
        FNativeHDRData originalData;
        glm::vec3 zenith(0.1f, 0.2f, 0.3f);
        glm::vec3 horizon(0.4f, 0.5f, 0.6f);
        glm::vec3 ground(0.05f, 0.05f, 0.05f);
        glm::vec3 sunColor(1.0f, 1.0f, 1.0f);
        glm::vec3 sunDir(0.0f, 1.0f, 0.0f);

        FHDRImporter::CreateAtmosphericHDR(32, 16, zenith, horizon, ground, sunColor, sunDir, 3.0f, originalData);

        std::string tempPath = "build/TempTest_HDR.lhdr";
        fs::create_directories("build");

        bool saveOk = originalData.SaveToFile(tempPath);
        CHECK(saveOk);
        CHECK(fs::exists(tempPath));

        FNativeHDRData loadedData;
        bool loadOk = loadedData.LoadFromFile(tempPath);
        CHECK(loadOk);
        CHECK(loadedData.Header.Magic == LHDR_MAGIC);
        CHECK(loadedData.Header.Version == LHDR_VERSION);
        CHECK(loadedData.Header.Width == originalData.Header.Width);
        CHECK(loadedData.Header.Height == originalData.Header.Height);
        CHECK(loadedData.Pixels.size() == originalData.Pixels.size());

        bool pixelsMatch = true;
        for (size_t i = 0; i < loadedData.Pixels.size(); ++i) {
            if (std::abs(loadedData.Pixels[i] - originalData.Pixels[i]) > 1e-6f) {
                pixelsMatch = false;
                break;
            }
        }
        CHECK(pixelsMatch);

        fs::remove(tempPath);
    }

    TEST_CASE("HDR - Corrupt Payload & Magic Rejection") {
        std::string corruptPath = "build/Corrupt_HDR.lhdr";
        fs::create_directories("build");

        {
            std::ofstream f(corruptPath, std::ios::binary);
            uint32_t badMagic = 0xDEADBEEF;
            f.write(reinterpret_cast<const char*>(&badMagic), sizeof(badMagic));
        }

        FNativeHDRData data;
        bool ok = data.LoadFromFile(corruptPath);
        CHECK_FALSE(ok);

        fs::remove(corruptPath);
    }

    TEST_CASE("HDR - Equirect UV matches SampleEquirectangular (+Y at v=0, +X at u=0.5)") {
        glm::vec3 plusY = EquirectDirectionFromUV(0.5f, 0.0f);
        CHECK(plusY.y == doctest::Approx(1.0f).epsilon(0.02f));
        CHECK(std::abs(plusY.x) < 0.05f);
        CHECK(std::abs(plusY.z) < 0.05f);

        glm::vec3 plusX = EquirectDirectionFromUV(0.5f, 0.5f);
        CHECK(plusX.x == doctest::Approx(1.0f).epsilon(0.02f));
        CHECK(std::abs(plusX.y) < 0.05f);
        CHECK(std::abs(plusX.z) < 0.05f);

        FNativeHDRData data;
        glm::vec3 zenith(0.08f, 0.28f, 0.52f);
        glm::vec3 horizon(0.22f, 0.55f, 0.70f);
        glm::vec3 ground(0.02f, 0.08f, 0.16f);
        REQUIRE(FHDRImporter::CreateAtmosphericHDR(64, 32, zenith, horizon, ground, glm::vec3(0.0f),
                                                   glm::vec3(0.0f, 1.0f, 0.0f), 0.0f, data));

        glm::vec3 sampledPlusY =
            SampleEquirectangular(data.Pixels.data(), static_cast<int>(data.Header.Width),
                                  static_cast<int>(data.Header.Height), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::vec3 expectedPlusY = SampleAtmosphericSky(glm::vec3(0.0f, 1.0f, 0.0f), zenith, horizon, ground);
        CHECK(sampledPlusY.r == doctest::Approx(expectedPlusY.r).epsilon(0.05f));
        CHECK(sampledPlusY.g == doctest::Approx(expectedPlusY.g).epsilon(0.05f));
        CHECK(sampledPlusY.b == doctest::Approx(expectedPlusY.b).epsilon(0.05f));

        glm::vec3 sampledPlusX =
            SampleEquirectangular(data.Pixels.data(), static_cast<int>(data.Header.Width),
                                  static_cast<int>(data.Header.Height), glm::vec3(1.0f, 0.0f, 0.0f));
        glm::vec3 expectedPlusX = SampleAtmosphericSky(glm::vec3(1.0f, 0.0f, 0.0f), zenith, horizon, ground);
        CHECK(sampledPlusX.r == doctest::Approx(expectedPlusX.r).epsilon(0.08f));
    }

}
