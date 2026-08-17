#include "Engine/UEngine.hpp"
#include "Gameplay/UClassRegistry.hpp"
#include "AShooterGameMode.hpp"
#include "AShooterGameState.hpp"
#include "AShooterCharacter.hpp"
#include "AShooterPlayerController.hpp"
#include "AShooterPlayerState.hpp"
#include "AShooterHUD.hpp"
#include "AShooterBotController.hpp"
#include "AShooterWeapon.hpp"
#include "UShooterGameInstance.hpp"
#include "FOpenGLRenderDriver.hpp"
#include "FJoltPhysicsDriver.hpp"

#include <cstdlib>
#include <filesystem>
#include <string>

namespace {

    std::string ResolveTournamentProjectFile(int argc, char** argv) {
        namespace fs = std::filesystem;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i] ? argv[i] : "";
            if (arg.rfind("--project=", 0) == 0)
                return arg.substr(10);
            if (arg.rfind("-project=", 0) == 0)
                return arg.substr(9);
            if ((arg == "--project" || arg == "-project") && i + 1 < argc && argv[i + 1])
                return argv[i + 1];
            if (arg.size() > 9 && arg.rfind(".lproject") == arg.size() - 9)
                return arg;
        }

        if (const char* env = std::getenv("LEON_PROJECT")) {
            if (env[0] != '\0')
                return env;
        }

        std::error_code ec;
        fs::path exeDir = fs::absolute(fs::path(argv[0]).parent_path(), ec);
        if (!ec) {
            for (const auto& entry : fs::directory_iterator(exeDir, ec)) {
                if (entry.path().extension() == ".lproject")
                    return entry.path().string();
            }
        }

        const char* candidates[] = {
            "LeonTournament.lproject",
            "Projects/LeonTournament/LeonTournament.lproject",
        };
        for (const char* c : candidates) {
            if (fs::exists(c))
                return c;
        }
        return "Projects/LeonTournament/LeonTournament.lproject";
    }

} // namespace

int main(int argc, char** argv) {
    Leon::FOpenGLRenderDriver::Register();
    Leon::FJoltPhysicsDriver::Register();

    auto& registry = Leon::UClassRegistry::Get();
    registry.RegisterClass<Leon::AShooterGameMode>("AShooterGameMode");
    registry.RegisterClass<Leon::AShooterGameState>("AShooterGameState");
    registry.RegisterClass<Leon::AShooterCharacter>("AShooterCharacter");
    registry.RegisterClass<Leon::AShooterPlayerController>("AShooterPlayerController");
    registry.RegisterClass<Leon::AShooterPlayerState>("AShooterPlayerState");
    registry.RegisterClass<Leon::AShooterHUD>("AShooterHUD");
    registry.RegisterClass<Leon::AShooterBotController>("AShooterBotController");
    registry.RegisterClass<Leon::AShooterRifle>("AShooterRifle");
    registry.RegisterClass<Leon::AShooterWeapon>("AShooterWeapon");

    Leon::UEngine::SetGameInstanceFactory(
        []() { return Leon::CreateRef<Leon::UShooterGameInstance>("ShooterGameInstance"); });

    Leon::FApplicationCommandLineArgs args{argc, argv};
    const std::string projectFile = ResolveTournamentProjectFile(argc, argv);
    return Leon::UEngine::Run(args, projectFile);
}
