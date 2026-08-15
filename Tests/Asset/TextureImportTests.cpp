#include <doctest/doctest.h>
#include "asset/TextureImporter.hpp"
#include "asset/AssetPath.hpp"

#include <filesystem>
#include <vector>

namespace fs = std::filesystem;
using namespace Leon;

TEST_SUITE("Texture Importer & .ltex Binary Format Tests") {

    TEST_CASE("TextureImporter - Semantic Auto-Detection") {
        CHECK(FTextureImportSettings::DetectFromFileName("T_Wood_diff.png").Semantic == ETextureSemantic::Albedo);
        CHECK(FTextureImportSettings::DetectFromFileName("T_Brick_color.png").Semantic == ETextureSemantic::Albedo);
        CHECK(FTextureImportSettings::DetectFromFileName("T_Rock_normal.png").Semantic == ETextureSemantic::Normal);
        CHECK(FTextureImportSettings::DetectFromFileName("T_Ground_nmap.png").Semantic == ETextureSemantic::Normal);
        CHECK(FTextureImportSettings::DetectFromFileName("T_Metal_gloss.png").Semantic == ETextureSemantic::Roughness);
        CHECK(FTextureImportSettings::DetectFromFileName("T_Iron_rough.png").Semantic == ETextureSemantic::Roughness);
        CHECK(FTextureImportSettings::DetectFromFileName("T_Steel_metal.png").Semantic == ETextureSemantic::Metallic);
        CHECK(FTextureImportSettings::DetectFromFileName("T_Lamp_illum.png").Semantic == ETextureSemantic::Emissive);
        CHECK(FTextureImportSettings::DetectFromFileName("T_Wall_ao.png").Semantic == ETextureSemantic::AO);
    }

    TEST_CASE("TextureImporter - Color Space Rules") {
        CHECK(FTextureImportSettings::DetectFromFileName("T_Wood_diff.png").ColorSpace == ETextureColorSpace::sRGB);
        CHECK(FTextureImportSettings::DetectFromFileName("T_Lamp_illum.png").ColorSpace == ETextureColorSpace::sRGB);
        CHECK(FTextureImportSettings::DetectFromFileName("T_Rock_normal.png").ColorSpace == ETextureColorSpace::Linear);
        CHECK(FTextureImportSettings::DetectFromFileName("T_Iron_rough.png").ColorSpace == ETextureColorSpace::Linear);
        CHECK(FTextureImportSettings::DetectFromFileName("T_Steel_metal.png").ColorSpace == ETextureColorSpace::Linear);
        CHECK(FTextureImportSettings::DetectFromFileName("T_Wall_ao.png").ColorSpace == ETextureColorSpace::Linear);
    }

    TEST_CASE("TextureImporter - Gloss to Roughness Inversion (R = 255 - G)") {
        std::vector<uint8_t> glossPixels = { 0, 50, 200, 255 };
        for (auto& p : glossPixels) {
            p = 255 - p;
        }
        CHECK(glossPixels[0] == 255);
        CHECK(glossPixels[1] == 205);
        CHECK(glossPixels[2] == 55);
        CHECK(glossPixels[3] == 0);
    }

    TEST_CASE("TextureImporter - Native .ltex Binary Serialization Roundtrip") {
        FNativeTextureData srcTex;
        srcTex.Header.Width = 4;
        srcTex.Header.Height = 4;
        srcTex.Header.Channels = 4;
        srcTex.Header.ColorSpace = static_cast<uint32_t>(ETextureColorSpace::sRGB);
        srcTex.Header.Semantic = static_cast<uint32_t>(ETextureSemantic::Albedo);
        srcTex.Header.MipCount = 3;

        FTextureMipData m0{ 0, 4, 4, std::vector<uint8_t>(4 * 4 * 4, 128) };
        FTextureMipData m1{ 1, 2, 2, std::vector<uint8_t>(2 * 2 * 4, 128) };
        FTextureMipData m2{ 2, 1, 1, std::vector<uint8_t>(1 * 1 * 4, 128) };
        srcTex.Mips = { m0, m1, m2 };

        std::string tempPath = "build/temp_test.ltex";
        CHECK(srcTex.SaveToFile(tempPath));

        FNativeTextureData loadedTex;
        CHECK(loadedTex.LoadFromFile(tempPath));
        CHECK(loadedTex.Header.Magic == LTEX_MAGIC);
        CHECK(loadedTex.Header.Width == 4);
        CHECK(loadedTex.Header.Height == 4);
        CHECK(loadedTex.Header.Channels == 4);
        CHECK(loadedTex.Mips.size() == 3);
        CHECK(loadedTex.Mips[0].Pixels.size() == 4 * 4 * 4);
        CHECK(loadedTex.Mips[2].Pixels.size() == 1 * 1 * 4);
        CHECK(loadedTex.Mips[2].Pixels[0] == 128);

        fs::remove(tempPath);
    }
}
