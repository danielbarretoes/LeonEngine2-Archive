#include <doctest/doctest.h>
#include "Assets/UAssetManager.hpp"
#include "Assets/UStaticMesh.hpp"

using namespace Leon;

TEST_SUITE("Asset cache UnloadUnused") {

    TEST_CASE("Dual-key cache entries unload when they are the only owners") {
        auto mesh = UStaticMesh::Create("P1UnloadDualKey");
        REQUIRE(mesh);
        UAssetManager::AddStaticMesh("__p1_unload_resolved.lmesh", mesh);
        UAssetManager::AddStaticMesh("/Game/P1Unload.lmesh", mesh);
        CHECK(UAssetManager::HasStaticMesh("__p1_unload_resolved.lmesh"));
        CHECK(UAssetManager::HasStaticMesh("/Game/P1Unload.lmesh"));

        mesh.reset();
        UAssetManager::UnloadUnused();
        CHECK_FALSE(UAssetManager::HasStaticMesh("__p1_unload_resolved.lmesh"));
        CHECK_FALSE(UAssetManager::HasStaticMesh("/Game/P1Unload.lmesh"));
    }

    TEST_CASE("External refs keep dual-key cache entries") {
        auto mesh = UStaticMesh::Create("P1UnloadKeep");
        UAssetManager::AddStaticMesh("__p1_keep_resolved.lmesh", mesh);
        UAssetManager::AddStaticMesh("/Game/P1Keep.lmesh", mesh);
        UAssetManager::UnloadUnused();
        CHECK(UAssetManager::HasStaticMesh("__p1_keep_resolved.lmesh"));
        CHECK(UAssetManager::HasStaticMesh("/Game/P1Keep.lmesh"));
        mesh.reset();
        UAssetManager::UnloadUnused();
        CHECK_FALSE(UAssetManager::HasStaticMesh("__p1_keep_resolved.lmesh"));
    }
}
