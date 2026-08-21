#include "Engine/IGameModule.hpp"

#include "ALeonTournamentAnimLabGameMode.hpp"
#include "ALeonTournamentBotController.hpp"
#include "ALeonTournamentCaptureTheFlagGameMode.hpp"
#include "ALeonTournamentCharacter.hpp"
#include "ALeonTournamentDummy.hpp"
#include "ALeonTournamentFlag.hpp"
#include "ALeonTournamentFlagBase.hpp"
#include "ALeonTournamentGameMode.hpp"
#include "ALeonTournamentGameState.hpp"
#include "ALeonTournamentHUD.hpp"
#include "ALeonTournamentPickup.hpp"
#include "ALeonTournamentPlayerController.hpp"
#include "ALeonTournamentPlayerState.hpp"
#include "ALeonTournamentProjectile.hpp"
#include "ALeonTournamentRenderLabGameMode.hpp"
#include "ALeonTournamentTransitionGameMode.hpp"
#include "ALeonTournamentTransitionHUD.hpp"
#include "ALeonTournamentWeapon.hpp"
#include "Engine/UIpNetDriver.hpp"
#include "FENetTransport.hpp"
#include "ULeonTournamentGameInstance.hpp"

LE_GAME_MODULE_EXPORT void LE_RegisterGameModule(Leon::UClassRegistry& InRegistry, Leon::FGameModuleHooks& OutHooks) {
    InRegistry.RegisterClass<Leon::ALeonTournamentGameMode>("ALeonTournamentGameMode");
    InRegistry.RegisterClass<Leon::ALeonTournamentGameState>("ALeonTournamentGameState");
    InRegistry.RegisterClass<Leon::ALeonTournamentCharacter>("ALeonTournamentCharacter");
    InRegistry.RegisterClass<Leon::ALeonTournamentPlayerController>("ALeonTournamentPlayerController");
    InRegistry.RegisterClass<Leon::ALeonTournamentPlayerState>("ALeonTournamentPlayerState");
    InRegistry.RegisterClass<Leon::ALeonTournamentHUD>("ALeonTournamentHUD");
    InRegistry.RegisterClass<Leon::ALeonTournamentBotController>("ALeonTournamentBotController");
    InRegistry.RegisterClass<Leon::ALeonTournamentRifle>("ALeonTournamentRifle");
    InRegistry.RegisterClass<Leon::ALeonTournamentShotgun>("ALeonTournamentShotgun");
    InRegistry.RegisterClass<Leon::ALeonTournamentRocketLauncher>("ALeonTournamentRocketLauncher");
    InRegistry.RegisterClass<Leon::ALeonTournamentLaserRifle>("ALeonTournamentLaserRifle");
    InRegistry.RegisterClass<Leon::ALeonTournamentFlamethrower>("ALeonTournamentFlamethrower");
    InRegistry.RegisterClass<Leon::ALeonTournamentWeapon>("ALeonTournamentWeapon");
    InRegistry.RegisterClass<Leon::ALeonTournamentProjectile>("ALeonTournamentProjectile");
    InRegistry.RegisterClass<Leon::ALeonTournamentWeaponPickup>("ALeonTournamentWeaponPickup");
    InRegistry.RegisterClass<Leon::ALeonTournamentHealthPickup>("ALeonTournamentHealthPickup");
    InRegistry.RegisterClass<Leon::ALeonTournamentJumpPad>("ALeonTournamentJumpPad");
    InRegistry.RegisterClass<Leon::ALeonTournamentDummy>("ALeonTournamentDummy");
    InRegistry.RegisterClass<Leon::ALeonTournamentAnimLabGameMode>("ALeonTournamentAnimLabGameMode");
    InRegistry.RegisterClass<Leon::ALeonTournamentRenderLabGameMode>("ALeonTournamentRenderLabGameMode");
    InRegistry.RegisterClass<Leon::ALeonTournamentCaptureTheFlagGameMode>("ALeonTournamentCaptureTheFlagGameMode");
    InRegistry.RegisterClass<Leon::ALeonTournamentFlag>("ALeonTournamentFlag");
    InRegistry.RegisterClass<Leon::ALeonTournamentFlagBase>("ALeonTournamentFlagBase");
    InRegistry.RegisterClass<Leon::ALeonTournamentTransitionGameMode>("ALeonTournamentTransitionGameMode");
    InRegistry.RegisterClass<Leon::ALeonTournamentTransitionHUD>("ALeonTournamentTransitionHUD");

    OutHooks.TransportFactorySetup = []() {
        Leon::UIpNetDriver::SetTransportFactory([]() { return std::make_unique<Leon::FENetTransport>(); });
    };
    OutHooks.GameInstanceFactory = []() {
        return Leon::CreateRef<Leon::ULeonTournamentGameInstance>("LeonTournamentGameInstance");
    };
}
