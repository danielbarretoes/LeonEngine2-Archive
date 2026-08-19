#include <doctest/doctest.h>

#include "Core/FConfigFile.hpp"
#include "Core/FProjectDescriptor.hpp"
#include "Core/FProjectPaths.hpp"
#include "Assets/FAssetPath.hpp"
#include "Engine/UEngine.hpp"

#include <filesystem>

namespace Leon {

    TEST_SUITE("Project Boot — .lproject / Multi-INI") {

        TEST_CASE("LocateProjectFile finds Sandbox.lproject from relative hint") {
            std::string found = FProjectPaths::LocateProjectFile("Projects/Sandbox/Sandbox.lproject");
            REQUIRE_FALSE(found.empty());
            CHECK(std::filesystem::exists(found));
            CHECK(found.find("Sandbox.lproject") != std::string::npos);
        }

        TEST_CASE("ResolveStartupMap prefers Engine.ini over .lproject") {
            FProjectDescriptor desc;
            desc.DefaultMap = "/Game/Maps/FromProject";

            FConfigFile engine;
            engine.SetString("/Script/EngineSettings.GameMapsSettings", "GameDefaultMap", "/Game/Maps/FromEngine");

            CHECK(UEngine::ResolveStartupMap(engine, desc) == "/Game/Maps/FromEngine");

            FConfigFile emptyEngine;
            CHECK(UEngine::ResolveStartupMap(emptyEngine, desc) == "/Game/Maps/FromProject");
        }

        TEST_CASE("BuildGameModeConfig uses Project.GameMode then Engine.GameModeBase") {
            FProjectDescriptor desc;
            desc.ProjectName = "MyGame";
            desc.DefaultGameMode = "AGameModeBase";

            FConfigFile engine;
            engine.SetString("/Script/EngineSettings.GameMapsSettings", "GlobalDefaultGameMode", "AGameModeBase");

            FConfigFile game;
            game.SetString("/Script/MyGame.GameMode", "GameModeClass", "AMyGameMode");
            game.SetString("/Script/MyGame.GameMode", "HUDClass", "AMyHUD");
            game.SetString("/Script/Engine.GameModeBase", "DefaultPawnClass", "ADefaultPawn");
            game.SetString("/Script/Engine.GameModeBase", "HUDClass", "AHUD");

            FGameModeConfig cfg = UEngine::BuildGameModeConfig(engine, game, desc);
            CHECK(cfg.GameModeClass == "AMyGameMode");
            CHECK(cfg.HUDClass == "AMyHUD");
            CHECK(cfg.DefaultPawnClass == "ADefaultPawn");
        }

        TEST_CASE("BuildGameModeConfig ignores obsolete SandboxGameMode INI section") {
            FProjectDescriptor desc;
            desc.ProjectName = "Sandbox";
            desc.DefaultGameMode = "AGameModeBase";

            FConfigFile engine;
            engine.SetString("/Script/EngineSettings.GameMapsSettings", "GlobalDefaultGameMode", "AGameModeBase");

            FConfigFile game;
            game.SetString("/Script/Sandbox.SandboxGameMode", "GameModeClass", "ASandboxGameMode");
            game.SetString("/Script/Sandbox.SandboxGameMode", "HUDClass", "ASandboxHUD");

            FGameModeConfig cfg = UEngine::BuildGameModeConfig(engine, game, desc);
            CHECK(cfg.GameModeClass == "AGameModeBase");
            CHECK(cfg.HUDClass == "AHUD");
        }

        TEST_CASE("Sandbox default map resolves on disk") {
            std::string project = FProjectPaths::LocateProjectFile("Projects/Sandbox/Sandbox.lproject");
            REQUIRE_FALSE(project.empty());
            FProjectPaths::SetProjectRoot(project);

            FConfigFile engine;
            REQUIRE(engine.Load(FAssetPath::Combine(FProjectPaths::ProjectConfigDir(), "DefaultEngine.ini")));
            FProjectDescriptor desc;
            REQUIRE(desc.Load(project));

            std::string mapVirt = UEngine::ResolveStartupMap(engine, desc);
            std::string physical = FProjectPaths::ResolveVirtualPath(mapVirt);
            REQUIRE(std::filesystem::exists(physical));
            CHECK(physical.find(".lmap") != std::string::npos);
            CHECK(std::filesystem::file_size(physical) > 0);
        }

    } // TEST_SUITE

} // namespace Leon
