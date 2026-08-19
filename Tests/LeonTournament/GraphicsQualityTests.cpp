#include <doctest/doctest.h>

#include "Engine/UWorld.hpp"
#include "FLeonTournamentGraphicsQuality.hpp"
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
        using Leon::ELeonTournamentGraphicsQuality;
        using Leon::FLeonTournamentGraphicsQuality;
        CHECK(FLeonTournamentGraphicsQuality::Parse("Low") == ELeonTournamentGraphicsQuality::Low);
        CHECK(FLeonTournamentGraphicsQuality::Parse("min") == ELeonTournamentGraphicsQuality::Low);
        CHECK(FLeonTournamentGraphicsQuality::Parse("Medium") == ELeonTournamentGraphicsQuality::Medium);
        CHECK(FLeonTournamentGraphicsQuality::Parse("recommended") == ELeonTournamentGraphicsQuality::Medium);
        CHECK(FLeonTournamentGraphicsQuality::Parse("High") == ELeonTournamentGraphicsQuality::High);
        CHECK(FLeonTournamentGraphicsQuality::Parse("optimal") == ELeonTournamentGraphicsQuality::High);
        CHECK(FLeonTournamentGraphicsQuality::Parse("hight") == ELeonTournamentGraphicsQuality::High);
        CHECK(std::string(FLeonTournamentGraphicsQuality::ToToken(ELeonTournamentGraphicsQuality::Low)) == "Low");
    }

    TEST_CASE("Presets match DefaultEngine.ini Low / Medium / High") {
        using Leon::ELeonTournamentGraphicsQuality;
        using Leon::EPlanarReflectionQuality;
        using Leon::EShadowFilterMode;
        using Leon::FLeonTournamentGraphicsQuality;

        const auto low = FLeonTournamentGraphicsQuality::GetPreset(ELeonTournamentGraphicsQuality::Low);
        CHECK(low.ShadowMapResolution == 1024);
        CHECK(low.CascadeCount == 2);
        CHECK(low.ShadowFilter == EShadowFilterMode::Hard);
        CHECK_FALSE(low.bEnablePlanarReflection);
        CHECK_FALSE(low.bEnableSSAO);
        CHECK_FALSE(low.bEnableBloom);
        CHECK(low.bEnableFXAA);

        const auto medium = FLeonTournamentGraphicsQuality::GetPreset(ELeonTournamentGraphicsQuality::Medium);
        CHECK(medium.ShadowMapResolution == 1024);
        CHECK(medium.CascadeCount == 3);
        CHECK(medium.bEnablePlanarReflection);
        CHECK(medium.PlanarQuality == EPlanarReflectionQuality::Low);
        CHECK(medium.bEnableSSAO);
        CHECK(medium.bEnableBloom);

        const auto high = FLeonTournamentGraphicsQuality::GetPreset(ELeonTournamentGraphicsQuality::High);
        CHECK(high.ShadowMapResolution == 2048);
        CHECK(high.CascadeCount == 4);
        CHECK(high.PlanarQuality == EPlanarReflectionQuality::Epic);
        CHECK(high.bEnableSSAO);
        CHECK(high.bEnableBloom);
    }

    TEST_CASE("ApplyToWorld writes pending renderer defaults") {
        using Leon::ELeonTournamentGraphicsQuality;
        using Leon::FLeonTournamentGraphicsQuality;

        auto world = Leon::UWorld::Create("GraphicsQualityWorld");
        REQUIRE(world != nullptr);
        FLeonTournamentGraphicsQuality::ApplyToWorld(*world, ELeonTournamentGraphicsQuality::Low);
        CHECK(world->GetPendingShadowMapResolution() == 1024);
        CHECK_FALSE(world->GetPendingPlanarReflectionEnabled());
        CHECK_FALSE(world->GetPendingSSAOEnabled());
        CHECK_FALSE(world->GetPendingBloomEnabled());
        CHECK(FLeonTournamentGraphicsQuality::InferFromWorld(*world) == ELeonTournamentGraphicsQuality::Low);

        FLeonTournamentGraphicsQuality::ApplyToWorld(*world, ELeonTournamentGraphicsQuality::High);
        CHECK(world->GetPendingShadowMapResolution() == 2048);
        CHECK(world->GetPendingPlanarReflectionEnabled());
        CHECK(FLeonTournamentGraphicsQuality::InferFromWorld(*world) == ELeonTournamentGraphicsQuality::High);
    }

    TEST_CASE("VRAM estimates increase Low < Medium < High") {
        using Leon::ELeonTournamentGraphicsQuality;
        using Leon::FLeonTournamentGraphicsQuality;
        const size_t low =
            FLeonTournamentGraphicsQuality::EstimateVRAMBytes(ELeonTournamentGraphicsQuality::Low, 1280, 720);
        const size_t medium =
            FLeonTournamentGraphicsQuality::EstimateVRAMBytes(ELeonTournamentGraphicsQuality::Medium, 1280, 720);
        const size_t high =
            FLeonTournamentGraphicsQuality::EstimateVRAMBytes(ELeonTournamentGraphicsQuality::High, 1280, 720);
        CHECK(low < medium);
        CHECK(medium < high);
        const std::string label =
            FLeonTournamentGraphicsQuality::FormatVRAMLabel(ELeonTournamentGraphicsQuality::High, 1280, 720);
        CHECK(label.find("HIGH") != std::string::npos);
        CHECK(label.find("MB") != std::string::npos);
    }

    TEST_CASE("PersistToIniFile patches live keys and keeps comments") {
        using Leon::ELeonTournamentGraphicsQuality;
        using Leon::FLeonTournamentGraphicsQuality;

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

        REQUIRE(FLeonTournamentGraphicsQuality::PersistToIniFile(ELeonTournamentGraphicsQuality::Low, path.string()));

        std::ifstream in(path);
        std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        in.close();
        CHECK(text.find("; keep this comment") != std::string::npos);
        CHECK(text.find("GraphicsQuality=Low") != std::string::npos);
        CHECK(text.find("ShadowMapResolution=1024") != std::string::npos);
        CHECK(text.find("EnablePlanarReflection=False") != std::string::npos);
        CHECK(text.find("EnableSSAO=False") != std::string::npos);
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }
}
