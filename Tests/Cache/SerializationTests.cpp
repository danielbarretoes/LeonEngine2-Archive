#include <doctest/doctest.h>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>
#include "renderer/IBLMath.hpp"

TEST_SUITE("Cache - IBL .libl Binary Serialization") {

    TEST_CASE("Round-Trip Bitwise Identical Serialization & Corrupt Header Rejection") {
        const std::string tempCachePath = "Engine/Assets/Textures/TempTestCache.libl";
        std::filesystem::create_directories("Engine/Assets/Textures");

        Leon::FIBLCacheHeader header;
        header.Version = 4;
        header.HDRSourceHash = 0x123456789ABCDEF0ull;
        header.EnvSize = 16;
        header.IrradSize = 8;
        header.PrefilterBaseSize = 16;
        header.PrefilterMips = 3;
        header.SampleCountIrradiance = 512;
        header.SampleCountPrefilter = 256;

        // Create deterministic test data
        std::vector<float> envData(6 * header.EnvSize * header.EnvSize * 4, 1.234f);
        std::vector<float> irradData(6 * header.IrradSize * header.IrradSize * 4, 5.678f);

        size_t prefTotalFloats = 0;
        for (uint32_t m = 0; m < header.PrefilterMips; ++m) {
            uint32_t s = header.PrefilterBaseSize >> m;
            prefTotalFloats += 6 * s * s * 4;
        }
        std::vector<float> prefData(prefTotalFloats, 9.1011f);

        // 1. Write to disk
        {
            std::ofstream out(tempCachePath, std::ios::binary);
            REQUIRE(out.is_open());
            out.write(reinterpret_cast<const char*>(&header), sizeof(Leon::FIBLCacheHeader));
            out.write(reinterpret_cast<const char*>(envData.data()), envData.size() * sizeof(float));
            out.write(reinterpret_cast<const char*>(irradData.data()), irradData.size() * sizeof(float));
            out.write(reinterpret_cast<const char*>(prefData.data()), prefData.size() * sizeof(float));
        }

        // 2. Read from disk and verify bitwise identical
        {
            std::ifstream in(tempCachePath, std::ios::binary);
            REQUIRE(in.is_open());

            Leon::FIBLCacheHeader readHeader;
            in.read(reinterpret_cast<char*>(&readHeader), sizeof(Leon::FIBLCacheHeader));

            CHECK(std::string(readHeader.Magic, 7) == "LEONIBL");
            CHECK(readHeader.Version == 4);
            CHECK(readHeader.HDRSourceHash == 0x123456789ABCDEF0ull);
            CHECK(readHeader.EnvSize == 16);
            CHECK(readHeader.IrradSize == 8);
            CHECK(readHeader.PrefilterBaseSize == 16);
            CHECK(readHeader.PrefilterMips == 3);

            std::vector<float> readEnv(envData.size());
            std::vector<float> readIrrad(irradData.size());
            std::vector<float> readPref(prefData.size());

            in.read(reinterpret_cast<char*>(readEnv.data()), readEnv.size() * sizeof(float));
            in.read(reinterpret_cast<char*>(readIrrad.data()), readIrrad.size() * sizeof(float));
            in.read(reinterpret_cast<char*>(readPref.data()), readPref.size() * sizeof(float));

            CHECK(std::memcmp(envData.data(), readEnv.data(), envData.size() * sizeof(float)) == 0);
            CHECK(std::memcmp(irradData.data(), readIrrad.data(), irradData.size() * sizeof(float)) == 0);
            CHECK(std::memcmp(prefData.data(), readPref.data(), prefData.size() * sizeof(float)) == 0);
        }

        // 3. Corrupt Magic check
        {
            std::ofstream out(tempCachePath, std::ios::binary);
            Leon::FIBLCacheHeader corruptHeader = header;
            corruptHeader.Magic[0] = 'B';
            corruptHeader.Magic[1] = 'A';
            corruptHeader.Magic[2] = 'D';
            out.write(reinterpret_cast<const char*>(&corruptHeader), sizeof(Leon::FIBLCacheHeader));
        }
        {
            std::ifstream in(tempCachePath, std::ios::binary);
            Leon::FIBLCacheHeader readHeader;
            in.read(reinterpret_cast<char*>(&readHeader), sizeof(Leon::FIBLCacheHeader));
            CHECK(std::string(readHeader.Magic, 7) != "LEONIBL");
        }

        // Cleanup
        std::filesystem::remove(tempCachePath);
    }
}
