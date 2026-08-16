#include "Engine/UEngine.hpp"
#include "Gameplay/UClassRegistry.hpp"
#include "ASandboxGameMode.hpp"
#include "ASandboxHUD.hpp"
#include "FOpenGLRenderDriver.hpp"

int main(int argc, char** argv) {
    Leon::FOpenGLRenderDriver::Register();

    // Register project-specific gameplay classes before the engine boots
    auto& registry = Leon::UClassRegistry::Get();
    registry.RegisterClass<Leon::ASandboxGameMode>("ASandboxGameMode");
    registry.RegisterClass<Leon::ASandboxHUD>("ASandboxHUD");

    Leon::FApplicationCommandLineArgs args{argc, argv};
    return Leon::UEngine::Run(args, "Projects/Sandbox/Sandbox.lproject");
}
