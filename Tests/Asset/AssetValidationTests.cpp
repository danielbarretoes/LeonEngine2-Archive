#include <doctest/doctest.h>
#include "Assets/FAssetManifest.hpp"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using namespace Leon;

TEST_SUITE("Asset Manifest & Dependency Validation Tests") {

    TEST_CASE("AssetManifest - 64-bit FNV-1a Hashing & Change Detection") {
        std::string testFile = "build/hash_test.tmp";
        {
            std::ofstream f(testFile);
            f << "Hello LeonEngine2 Asset Pipeline!";
        }

        std::string hash1 = FAssetManifest::ComputeFileHash(testFile);
        CHECK(!hash1.empty());

        // Same content -> same hash
        std::string hash2 = FAssetManifest::ComputeFileHash(testFile);
        CHECK(hash1 == hash2);

        // Modified content -> changed hash
        {
            std::ofstream f(testFile);
            f << "Modified Content!";
        }
        std::string hash3 = FAssetManifest::ComputeFileHash(testFile);
        CHECK(hash1 != hash3);

        fs::remove(testFile);
    }

    TEST_CASE("AssetManifest - Serialization Roundtrip") {
        FAssetManifest manifest;
        manifest.RegisterImport("Raw/SampleMesh.fbx", { "Meshes/SampleMesh.lmesh", "Materials/M_SampleMesh.lmat" }, { "Materials/M_SampleMesh.lmat" });

        std::string manifestFile = "build/manifest_test.json";
        CHECK(manifest.SaveToFile(manifestFile));

        FAssetManifest loadedManifest;
        CHECK(loadedManifest.LoadFromFile(manifestFile));
        CHECK(loadedManifest.GetEntries().size() == 1);
        auto it = loadedManifest.GetEntries().find("Raw/SampleMesh.fbx");
        CHECK(it != loadedManifest.GetEntries().end());
        CHECK(it->second.GeneratedAssets.size() == 2);
        CHECK(it->second.Dependencies.size() == 1);

        fs::remove(manifestFile);
    }
}
