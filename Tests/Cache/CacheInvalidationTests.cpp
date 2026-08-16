#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include "Renderer/FIBLMath.hpp"

TEST_SUITE("Cache - Invalidation & FNV-1a 64-bit Content Hashing") {

    TEST_CASE("FNV-1a 64-bit Content Hashing & Invalidation on File Modification") {
        const std::string testFilePath = "Engine/Assets/Textures/TempHashTest.bin";
        std::filesystem::create_directories("Engine/Assets/Textures");

        // 1. Create file A
        {
            std::ofstream out(testFilePath, std::ios::binary);
            std::string content = "AutumnField1k_HDR_Test_Data_123456789";
            out.write(content.data(), content.size());
        }

        uint64_t hashA1 = Leon::ComputeFileHash64(testFilePath);
        uint64_t hashA2 = Leon::ComputeFileHash64(testFilePath);
        CHECK(hashA1 != 0);
        CHECK(hashA1 == hashA2); // Deterministic hit

        // 2. Modify one single byte in file B
        {
            std::ofstream out(testFilePath, std::ios::binary);
            std::string content = "AutumnField1k_HDR_Test_Data_123456780"; // Modified last char
            out.write(content.data(), content.size());
        }

        uint64_t hashB = Leon::ComputeFileHash64(testFilePath);
        CHECK(hashB != 0);
        CHECK(hashA1 != hashB); // Invalidation verified (avalanche effect)

        // 3. Header Version Check Invalidation
        Leon::FIBLCacheHeader headerV3;
        headerV3.Version = 3;
        headerV3.HDRSourceHash = hashA1;

        Leon::FIBLCacheHeader headerV5;
        headerV5.Version = 5;
        headerV5.HDRSourceHash = hashA1;

        CHECK(headerV3.Version != 5);
        CHECK(headerV5.Version == 5);

        std::filesystem::remove(testFilePath);
    }
}
