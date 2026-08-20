#include <doctest/doctest.h>

#include "Engine/UWorld.hpp"
#include "Engine/FGraphicsQuality.hpp"
#include "RHI/FRenderer.hpp"
#include "Assets/UAssetManager.hpp"
#include "Renderer/FShadowTypes.hpp"
#include "Renderer/FPlanarReflectionTypes.hpp"

#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

TEST_SUITE("LeonTournament graphics quality") {

    TEST_CASE("Parse accepts menu tokens and INI aliases") {
        using Leon::EGraphicsQuality;
        using Leon::FGraphicsQuality;
        CHECK(FGraphicsQuality::Parse("Low") == EGraphicsQuality::Low);
        CHECK(FGraphicsQuality::Parse("min") == EGraphicsQuality::Low);
        CHECK(FGraphicsQuality::Parse("Medium") == EGraphicsQuality::Medium);
        CHECK(FGraphicsQuality::Parse("recommended") == EGraphicsQuality::Medium);
        CHECK(FGraphicsQuality::Parse("High") == EGraphicsQuality::High);
        CHECK(FGraphicsQuality::Parse("optimal") == EGraphicsQuality::High);
        CHECK(FGraphicsQuality::Parse("hight") == EGraphicsQuality::High);
        CHECK(std::string(FGraphicsQuality::ToToken(EGraphicsQuality::Low)) == "Low");
    }

    TEST_CASE("Presets match DefaultEngine.ini Low / Medium / High") {
        using Leon::EGraphicsQuality;
        using Leon::EPlanarReflectionQuality;
        using Leon::EShadowFilterMode;
        using Leon::FGraphicsQuality;

        const auto low = FGraphicsQuality::GetPreset(EGraphicsQuality::Low);
        CHECK(low.ShadowMapResolution == 512);
        CHECK(low.CascadeCount == 1);
        CHECK(low.ShadowFilter == EShadowFilterMode::Hard);
        CHECK_FALSE(low.bEnablePlanarReflection);
        CHECK_FALSE(low.bEnableSSAO);
        CHECK_FALSE(low.bEnableBloom);
        CHECK(low.ShadowDistance == 12.0f);
        CHECK(low.SpotResolution == 512);
        CHECK(low.PointShadowResolution == 256);
        CHECK(low.MaxShadowedPointLights == 1);
        CHECK_FALSE(low.bEnableFXAA);
        CHECK(low.MaxTextureResolution == 256);

        const auto medium = FGraphicsQuality::GetPreset(EGraphicsQuality::Medium);
        CHECK(medium.ShadowMapResolution == 1024);
        CHECK(medium.CascadeCount == 3);
        CHECK(medium.SpotResolution == 1024);
        CHECK(medium.PointShadowResolution == 512);
        CHECK(medium.MaxShadowedPointLights == 2);
        CHECK(medium.bEnablePlanarReflection);
        CHECK(medium.PlanarQuality == EPlanarReflectionQuality::Low);
        CHECK(medium.bEnableSSAO);
        CHECK(medium.bEnableBloom);
        CHECK(medium.MaxTextureResolution == 512);

        const auto high = FGraphicsQuality::GetPreset(EGraphicsQuality::High);
        CHECK(high.ShadowMapResolution == 2048);
        CHECK(high.CascadeCount == 4);
        CHECK(high.SpotResolution == 1024);
        CHECK(high.PointShadowResolution == 512);
        CHECK(high.MaxShadowedPointLights == 4);
        CHECK(high.ShadowFilter == Leon::EShadowFilterMode::PCF5x5);
        CHECK(high.PlanarQuality == EPlanarReflectionQuality::Epic);
        CHECK(high.bEnableSSAO);
        CHECK(high.bEnableBloom);
        CHECK(high.MaxTextureResolution == 1024);
    }

    TEST_CASE("ApplyToWorld sets max texture resolution") {
        using Leon::EGraphicsQuality;
        using Leon::FGraphicsQuality;
        using Leon::FRenderer;

        auto world = Leon::UWorld::Create("TextureQualityWorld");
        REQUIRE(world != nullptr);

        FGraphicsQuality::ApplyToWorld(*world, EGraphicsQuality::Low);
        CHECK(FRenderer::GetMaxTextureResolution() == 256);

        FGraphicsQuality::ApplyToWorld(*world, EGraphicsQuality::Medium);
        CHECK(FRenderer::GetMaxTextureResolution() == 512);

        FGraphicsQuality::ApplyToWorld(*world, EGraphicsQuality::High);
        CHECK(FRenderer::GetMaxTextureResolution() == 1024);
    }

    TEST_CASE("ReloadAllTextures keeps max resolution and does not crash headless") {
        using Leon::EGraphicsQuality;
        using Leon::FGraphicsQuality;
        using Leon::FRenderer;
        using Leon::UAssetManager;

        auto world = Leon::UWorld::Create("TextureReloadWorld");
        REQUIRE(world != nullptr);
        FGraphicsQuality::ApplyToWorld(*world, EGraphicsQuality::Low);
        CHECK(FRenderer::GetMaxTextureResolution() == 256);
        UAssetManager::ReloadAllTextures(world.get());
        CHECK(FRenderer::GetMaxTextureResolution() == 256);
    }

    TEST_CASE("ApplyToWorld writes pending renderer defaults") {
        using Leon::EGraphicsQuality;
        using Leon::FGraphicsQuality;

        auto world = Leon::UWorld::Create("GraphicsQualityWorld");
        REQUIRE(world != nullptr);
        FGraphicsQuality::ApplyToWorld(*world, EGraphicsQuality::Low);
        CHECK(world->GetPendingShadowMapResolution() == 512);
        CHECK(world->GetPendingSpotResolution() == 512);
        CHECK(world->GetPendingPointShadowResolution() == 256);
        CHECK(world->GetPendingMaxShadowedPointLights() == 1);
        CHECK_FALSE(world->GetPendingPlanarReflectionEnabled());
        CHECK_FALSE(world->GetPendingSSAOEnabled());
        CHECK_FALSE(world->GetPendingBloomEnabled());
        CHECK(FGraphicsQuality::InferFromWorld(*world) == EGraphicsQuality::Low);

        FGraphicsQuality::ApplyToWorld(*world, EGraphicsQuality::High);
        CHECK(world->GetPendingShadowMapResolution() == 2048);
        CHECK(world->GetPendingPlanarReflectionEnabled());
        CHECK(FGraphicsQuality::InferFromWorld(*world) == EGraphicsQuality::High);
    }

    TEST_CASE("VRAM estimates increase Low < Medium < High") {
        using Leon::EGraphicsQuality;
        using Leon::FGraphicsQuality;
        const size_t low = FGraphicsQuality::EstimateVRAMBytes(EGraphicsQuality::Low, 1280, 720);
        const size_t medium = FGraphicsQuality::EstimateVRAMBytes(EGraphicsQuality::Medium, 1280, 720);
        const size_t high = FGraphicsQuality::EstimateVRAMBytes(EGraphicsQuality::High, 1280, 720);
        CHECK(low < medium);
        CHECK(medium < high);
        const std::string label = FGraphicsQuality::FormatVRAMLabel(EGraphicsQuality::High, 1280, 720);
        CHECK(label.find("HIGH") != std::string::npos);
        CHECK(label.find("Tex 1024px") != std::string::npos);
        CHECK(label.find("MB") != std::string::npos);
    }

    TEST_CASE("FormatPresetLabel exposes texture limits") {
        using Leon::EGraphicsQuality;
        using Leon::FGraphicsQuality;
        CHECK(FGraphicsQuality::FormatPresetLabel(EGraphicsQuality::Low).find("Tex 256px") != std::string::npos);
        CHECK(FGraphicsQuality::FormatPresetLabel(EGraphicsQuality::Medium).find("Tex 512px") != std::string::npos);
        CHECK(FGraphicsQuality::FormatPresetLabel(EGraphicsQuality::High).find("Tex 1024px") != std::string::npos);
    }

    TEST_CASE("PersistToIniFile patches live keys and keeps comments") {
        using Leon::EGraphicsQuality;
        using Leon::FGraphicsQuality;

        const auto path = std::filesystem::temp_directory_path() /
                          ("leon_graphics_quality_test_" + std::to_string(std::time(nullptr)) + ".ini");
        {
            std::ofstream out(path);
            out << "[/Script/Engine.RendererSettings]\n";
            out << "; keep this comment\n";
            out << "ShadowMapResolution=2048\n";
            out << "CascadeCount=4\n";
            out << "ShadowDistance=100\n";
            out << "ShadowFilter=PCF3x3\n";
            out << "EnablePlanarReflection=True\n";
            out << "PlanarReflectionQuality=Epic\n";
            out << "EnableSSAO=True\n";
            out << "EnableBloom=True\n";
            out << "EnableFXAA=True\n";
        }

        REQUIRE(FGraphicsQuality::PersistToIniFile(EGraphicsQuality::Low, path.string()));

        std::ifstream in(path);
        std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        in.close();
        CHECK(text.find("; keep this comment") != std::string::npos);
        CHECK(text.find("GraphicsQuality=Low") != std::string::npos);
        CHECK(text.find("MaxTextureResolution=256") != std::string::npos);
        CHECK(text.find("ShadowMapResolution=512") != std::string::npos);
        CHECK(text.find("EnablePlanarReflection=False") != std::string::npos);
        CHECK(text.find("EnableSSAO=False") != std::string::npos);
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }
}
