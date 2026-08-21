#include <doctest/doctest.h>

#include "Gameplay/AActor.hpp"
#include "Gameplay/APawn.hpp"
#include "Gameplay/ACharacter.hpp"
#include "Gameplay/UClassRegistry.hpp"
#include "Gameplay/UHealthComponent.hpp"
#include "Lightmass/FLightBuildSettings.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

    bool IsSkippedDir(const fs::path& InPath) {
        for (const auto& part : InPath) {
            const std::string name = part.string();
            if (name == ".git" || name == "build" || name == "out" || name == "ThirdParty" || name == ".vs" ||
                name == "CMakeFiles" || name == "__pycache__")
                return true;
        }
        return false;
    }

    bool HeaderDeclaresPrimaryType(const std::string& InStem, const std::string& InText) {
        if (InStem == "Components" || InStem.ends_with("Types") || InStem.ends_with("Widgets"))
            return true;
        const std::string classDecl = "class " + InStem;
        const std::string structDecl = "struct " + InStem;
        const std::string enumDecl = "enum class " + InStem;
        return InText.find(classDecl) != std::string::npos || InText.find(structDecl) != std::string::npos ||
               InText.find(enumDecl) != std::string::npos;
    }

} // namespace

TEST_SUITE("Naming conventions") {

    TEST_CASE("canonical Unreal-style engine APIs") {
        using namespace Leon;
        CHECK(UClassRegistry::Get().HasClass("AActor"));
        CHECK(UClassRegistry::Get().HasClass("APawn"));
        CHECK(UClassRegistry::Get().HasClass("ACharacter"));
        CHECK(UClassRegistry::Get().HasClass("AGameModeBase"));
        CHECK(UClassRegistry::Get().HasClass("AGameMode"));
        CHECK(UClassRegistry::Get().HasClass("AGameStateBase"));
        CHECK(UClassRegistry::Get().HasClass("AGameState"));
        CHECK(UClassRegistry::Get().HasClass("APlayerController"));
        CHECK(UClassRegistry::Get().HasClass("AHUD"));
        CHECK(UClassRegistry::Get().HasClass("ABlockingVolume"));
        CHECK(UClassRegistry::Get().HasClass("APhysicsVolume"));
        CHECK(UClassRegistry::Get().HasClass("ASkyLight"));
        CHECK(UClassRegistry::Get().HasClass("ATriggerVolume"));
        CHECK(UClassRegistry::Get().HasClass("ANavMeshBoundsVolume"));
        CHECK(UClassRegistry::Get().HasClass("AProjectile"));

        CHECK_FALSE(UClassRegistry::Get().HasClass("Actor"));
        CHECK_FALSE(UClassRegistry::Get().HasClass("Pawn"));
        CHECK_FALSE(UClassRegistry::Get().HasClass("HUD"));
        CHECK_FALSE(UClassRegistry::Get().HasClass("DefaultGameMode"));
        CHECK_FALSE(UClassRegistry::Get().HasClass("DefaultPlayerController"));

        auto settings = FLightBuildSettings::Medium();
        CHECK(settings.Resolution == 64);

        UHealthComponent health;
        CHECK_FALSE(health.IsDead());
        health.SetHealth(0.0f);
        CHECK(health.IsDead());
    }

    TEST_CASE("A/U public headers match primary type names") {
        const fs::path root = "Engine/Source/Runtime";
        REQUIRE(fs::exists(root));
        int mismatches = 0;
        for (const auto& entry : fs::recursive_directory_iterator(root)) {
            if (!entry.is_regular_file() || IsSkippedDir(entry.path()))
                continue;
            if (entry.path().extension() != ".hpp")
                continue;
            const std::string stem = entry.path().stem().string();
            if (stem.size() < 2)
                continue;
            const char prefix = stem.front();
            if (prefix != 'A' && prefix != 'U')
                continue;
            if (stem == "Components" || stem.ends_with("Widgets"))
                continue;
            std::ifstream file(entry.path());
            std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            if (!HeaderDeclaresPrimaryType(stem, text)) {
                ++mismatches;
                const std::string path = entry.path().generic_string();
                CHECK_MESSAGE(false, path);
            }
        }
        CHECK(mismatches == 0);
    }

    TEST_CASE("legacy Shooter and ULightBuildSettings names are gone") {
        const char* forbidden[] = {
            "AShooter",       "UShooter",   "FShooter",          "EShooter", "ULightBuildSettings",
            "GetIsAuthority", "GetShooter", "GetTournamentPawn",
        };

        const fs::path roots[] = {fs::path("Engine"), fs::path("Projects"), fs::path("Tests"), fs::path("Plugins")};
        int hits = 0;
        for (const auto& scanRoot : roots) {
            REQUIRE(fs::exists(scanRoot));
            for (const auto& entry : fs::recursive_directory_iterator(scanRoot)) {
                if (!entry.is_regular_file() || IsSkippedDir(entry.path()))
                    continue;
                const auto ext = entry.path().extension();
                if (ext != ".hpp" && ext != ".h" && ext != ".cpp" && ext != ".ini" && ext != ".lproject")
                    continue;
                const std::string generic = entry.path().generic_string();
                if (generic.find("EngineGameSeparationTests") != std::string::npos)
                    continue;
                if (generic.find("NamingConventionTests") != std::string::npos)
                    continue;
                std::ifstream file(entry.path());
                std::string line;
                while (std::getline(file, line)) {
                    for (const char* token : forbidden) {
                        if (line.find(token) != std::string::npos) {
                            ++hits;
                            const std::string msg = generic + " contains " + token;
                            CHECK_MESSAGE(false, msg.c_str());
                        }
                    }
                }
            }
        }
        CHECK(hits == 0);
    }
}
