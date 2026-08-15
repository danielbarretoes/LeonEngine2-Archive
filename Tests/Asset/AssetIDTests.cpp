#include <doctest/doctest.h>
#include "asset/AssetPath.hpp"
#include "asset/AssetTypes.hpp"

using namespace Leon;

TEST_SUITE("AssetPath & Asset Identification Tests") {

    TEST_CASE("AssetPath - Path Normalization") {
        CHECK(FAssetPath::Normalize("Projects\\Sandbox\\Content\\Raw\\Mesh.fbx") ==
              "Projects/Sandbox/Content/Raw/Mesh.fbx");
        CHECK(FAssetPath::Normalize("C:\\Engine//Assets///Shaders\\\\PBR_Lit.glsl") ==
              "C:/Engine/Assets/Shaders/PBR_Lit.glsl");
        CHECK(FAssetPath::Normalize("") == "");
    }

    TEST_CASE("AssetPath - Extension & Filename Extraction") {
        std::string path = "Projects/Sandbox/Content/Textures/T_Wood_D.ltex";
        CHECK(FAssetPath::GetExtension(path) == "ltex");
        CHECK(FAssetPath::GetFileName(path) == "T_Wood_D.ltex");
        CHECK(FAssetPath::GetFileNameWithoutExtension(path) == "T_Wood_D");
        CHECK(FAssetPath::GetDirectory(path) == "Projects/Sandbox/Content/Textures");
    }

    TEST_CASE("AssetPath - Virtual Path Resolution") {
        std::string root = "Projects/Sandbox/Content";
        std::string full = "Projects/Sandbox/Content/Meshes/Cube.lmesh";
        CHECK(FAssetPath::MakeVirtualPath(root, full) == "Meshes/Cube.lmesh");
    }

    TEST_CASE("AssetPath - Path Combining") {
        CHECK(FAssetPath::Combine("Projects/Sandbox", "Content/Textures") == "Projects/Sandbox/Content/Textures");
        CHECK(FAssetPath::Combine("Projects/Sandbox/", "/Content/Textures") == "Projects/Sandbox/Content/Textures");
    }

    TEST_CASE("AssetID - UUID Generation and Formatting") {
        FUUID uuid1 = FUUID::Generate();
        FUUID uuid2 = FUUID::Generate();
        CHECK(uuid1.IsValid());
        CHECK(uuid2.IsValid());
        CHECK(uuid1 != uuid2);
        std::string str = uuid1.ToString();
        CHECK(str.length() == 32);
        FUUID parsed = FUUID::FromString(str);
        CHECK(parsed == uuid1);
    }
}
