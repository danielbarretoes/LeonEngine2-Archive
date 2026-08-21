#include "Engine/IGameModule.hpp"
#include "ASandboxDemoPickup.hpp"
#include "ASandboxGameMode.hpp"
#include "ASandboxHUD.hpp"

LE_GAME_MODULE_EXPORT void LE_RegisterGameModule(Leon::UClassRegistry& InRegistry, Leon::FGameModuleHooks& OutHooks) {
    InRegistry.RegisterClass<Leon::ASandboxGameMode>("ASandboxGameMode");
    InRegistry.RegisterClass<Leon::ASandboxHUD>("ASandboxHUD");
    InRegistry.RegisterClass<Leon::ASandboxDemoPickup>("ASandboxDemoPickup");
    (void)OutHooks;
}
