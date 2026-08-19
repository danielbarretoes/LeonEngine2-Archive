#include <doctest/doctest.h>

#include "Gameplay/UClassRegistry.hpp"
#include "ALeonTournamentRenderLabGameMode.hpp"
#include "LeonTournamentTestSetup.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

TEST_SUITE("LeonTournament render lab") {

    TEST_CASE("RenderLab map asset exists") {
        Leon::Test::BindLeonTournamentProject();
        CHECK(std::filesystem::exists("Projects/LeonTournament/Content/Maps/RenderLab.lmap"));
    }

    TEST_CASE("RenderLab camera yaw looks down -Z") {
        std::ifstream file("Projects/LeonTournament/Content/Maps/RenderLab.lmap");
        REQUIRE(file.good());
        std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        CHECK(contents.find("Rotation: [-16, -90, 0]") != std::string::npos);
        CHECK(contents.find("Lab Camera B") != std::string::npos);
        CHECK(contents.find("Lab Camera C") != std::string::npos);
        CHECK(contents.find("DaySky1k.lhdr") != std::string::npos);
        CHECK(contents.find("UseHDREnvironmentMap: true") != std::string::npos);
    }

    TEST_CASE("RenderLab game mode is registered by name") {
        auto& r = Leon::UClassRegistry::Get();
        r.RegisterClass<Leon::ALeonTournamentRenderLabGameMode>("ALeonTournamentRenderLabGameMode");
        CHECK(r.HasClass("ALeonTournamentRenderLabGameMode"));
        Leon::ALeonTournamentRenderLabGameMode gm;
        CHECK(gm.WantsUICursor());
        Leon::ALeonTournamentGameMode match;
        CHECK_FALSE(match.WantsUICursor());
    }
}
