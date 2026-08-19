#include "Engine/UEngine.hpp"
#include "Gameplay/UClassRegistry.hpp"
#include "ALeonTournamentGameMode.hpp"
#include "ALeonTournamentGameState.hpp"
#include "ALeonTournamentCharacter.hpp"
#include "ALeonTournamentPlayerController.hpp"
#include "ALeonTournamentPlayerState.hpp"
#include "ALeonTournamentHUD.hpp"
#include "ALeonTournamentBotController.hpp"
#include "ALeonTournamentWeapon.hpp"
#include "ALeonTournamentProjectile.hpp"
#include "ALeonTournamentPickup.hpp"
#include "ALeonTournamentDummy.hpp"
#include "ALeonTournamentAnimLabGameMode.hpp"
#include "ALeonTournamentRenderLabGameMode.hpp"
#include "ALeonTournamentTransitionGameMode.hpp"
#include "ALeonTournamentTransitionHUD.hpp"
#include "ALeonTournamentCaptureTheFlagGameMode.hpp"
#include "ALeonTournamentFlag.hpp"
#include "ALeonTournamentFlagBase.hpp"
#include "ULeonTournamentGameInstance.hpp"
#include "Engine/UIpNetDriver.hpp"
#include "FOpenGLRenderDriver.hpp"
#include "FJoltPhysicsDriver.hpp" // link Leon::Jolt — auto-registers IPhysicsScene factory
#include "FENetTransport.hpp"

#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <memory>
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
    Leon::UIpNetDriver::SetTransportFactory([]() { return std::make_unique<Leon::FENetTransport>(); });

    auto& registry = Leon::UClassRegistry::Get();
    registry.RegisterClass<Leon::ALeonTournamentGameMode>("ALeonTournamentGameMode");
    registry.RegisterClass<Leon::ALeonTournamentGameState>("ALeonTournamentGameState");
    registry.RegisterClass<Leon::ALeonTournamentCharacter>("ALeonTournamentCharacter");
    registry.RegisterClass<Leon::ALeonTournamentPlayerController>("ALeonTournamentPlayerController");
    registry.RegisterClass<Leon::ALeonTournamentPlayerState>("ALeonTournamentPlayerState");
    registry.RegisterClass<Leon::ALeonTournamentHUD>("ALeonTournamentHUD");
    registry.RegisterClass<Leon::ALeonTournamentBotController>("ALeonTournamentBotController");
    registry.RegisterClass<Leon::ALeonTournamentRifle>("ALeonTournamentRifle");
    registry.RegisterClass<Leon::ALeonTournamentShotgun>("ALeonTournamentShotgun");
    registry.RegisterClass<Leon::ALeonTournamentRocketLauncher>("ALeonTournamentRocketLauncher");
    registry.RegisterClass<Leon::ALeonTournamentLaserRifle>("ALeonTournamentLaserRifle");
    registry.RegisterClass<Leon::ALeonTournamentFlamethrower>("ALeonTournamentFlamethrower");
    registry.RegisterClass<Leon::ALeonTournamentWeapon>("ALeonTournamentWeapon");
    registry.RegisterClass<Leon::ALeonTournamentProjectile>("ALeonTournamentProjectile");
    registry.RegisterClass<Leon::ALeonTournamentWeaponPickup>("ALeonTournamentWeaponPickup");
    registry.RegisterClass<Leon::ALeonTournamentHealthPickup>("ALeonTournamentHealthPickup");
    registry.RegisterClass<Leon::ALeonTournamentJumpPad>("ALeonTournamentJumpPad");
    registry.RegisterClass<Leon::ALeonTournamentDummy>("ALeonTournamentDummy");
    registry.RegisterClass<Leon::ALeonTournamentAnimLabGameMode>("ALeonTournamentAnimLabGameMode");
    registry.RegisterClass<Leon::ALeonTournamentRenderLabGameMode>("ALeonTournamentRenderLabGameMode");
    registry.RegisterClass<Leon::ALeonTournamentCaptureTheFlagGameMode>("ALeonTournamentCaptureTheFlagGameMode");
    registry.RegisterClass<Leon::ALeonTournamentFlag>("ALeonTournamentFlag");
    registry.RegisterClass<Leon::ALeonTournamentFlagBase>("ALeonTournamentFlagBase");
    registry.RegisterClass<Leon::ALeonTournamentTransitionGameMode>("ALeonTournamentTransitionGameMode");
    registry.RegisterClass<Leon::ALeonTournamentTransitionHUD>("ALeonTournamentTransitionHUD");

    bool autoOffline = false;
    bool animLab = false;
    float validateSeconds = 65.0f;
    std::string reportPath;
    int32_t botsTeam1 = -1;
    int32_t botsTeam2 = -1;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i] ? argv[i] : "";
        if (arg == "--offline-match" || arg == "--benchmark")
            autoOffline = true;
        else if (arg == "--anim-lab")
            animLab = true;
        else if (arg.rfind("--validate-seconds=", 0) == 0)
            validateSeconds = std::strtof(arg.c_str() + 19, nullptr);
        else if (arg.rfind("--report=", 0) == 0)
            reportPath = arg.substr(9);
        else if (arg.rfind("--bots=", 0) == 0) {
            const char* v = arg.c_str() + 7;
            char* end = nullptr;
            botsTeam1 = static_cast<int32_t>(std::strtol(v, &end, 10));
            if (end && *end == ',')
                botsTeam2 = static_cast<int32_t>(std::strtol(end + 1, nullptr, 10));
        }
    }

    if (animLab)
        Leon::UEngine::SetStartupOverrides("/Game/Maps/AnimLab", "ALeonTournamentAnimLabGameMode");

    Leon::UEngine::SetGameInstanceFactory([autoOffline, validateSeconds, reportPath, botsTeam1, botsTeam2]() {
        auto gi = Leon::CreateRef<Leon::ULeonTournamentGameInstance>("LeonTournamentGameInstance");
        if (autoOffline)
            gi->ConfigureAutoOfflineMatch(validateSeconds, reportPath);
        if (botsTeam1 >= 0)
            gi->SetDesiredBotsTeam1(botsTeam1);
        if (botsTeam2 >= 0)
            gi->SetDesiredBotsTeam2(botsTeam2);
        return gi;
    });

    Leon::FApplicationCommandLineArgs args{argc, argv};
    const std::string projectFile = ResolveTournamentProjectFile(argc, argv);
    return Leon::UEngine::Run(args, projectFile);
}
