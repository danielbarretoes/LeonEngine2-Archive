#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>
#include "Assets/UAssetManager.hpp"
#include "RHI/FRenderer.hpp"
#include "FOpenGLRenderDriver.hpp"
#include "FJoltPhysicsDriver.hpp"
#include "FENetTransport.hpp"
#include "Engine/UIpNetDriver.hpp"
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

#include "LeonTournamentTestSetup.hpp"

#include <cstdlib>

int main(int argc, char** argv) {
    Leon::Test::BindLeonTournamentProject();
    Leon::UIpNetDriver::SetTransportFactory([]() { return std::make_unique<Leon::FENetTransport>(); });
    Leon::FJoltPhysicsDriver::Register();

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

    doctest::Context context;
    context.applyCommandLine(argc, argv);
    int res = context.run();
    Leon::UAssetManager::Shutdown();
    Leon::FRenderer::Shutdown();
    if (context.shouldExit())
        return res;
    std::quick_exit(res);
}
