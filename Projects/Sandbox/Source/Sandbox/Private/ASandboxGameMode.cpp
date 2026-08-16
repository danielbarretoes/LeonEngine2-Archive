#include "ASandboxGameMode.hpp"
#include "ASandboxHUD.hpp"

namespace Leon {

    ASandboxGameMode::ASandboxGameMode(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AGameModeBase(InHandle, InWorld, InName) {
        HUDClass = "ASandboxHUD";
        DefaultPawnClass = "ADefaultPawn";
        PlayerControllerClass = "APlayerController";
        GameStateClass = "AGameStateBase";
        PlayerStateClass = "APlayerState";
    }

    void ASandboxGameMode::InitGame() {
        // Ensure Sandbox HUD is selected even if INI omitted HUDClass
        if (HUDClass.empty() || HUDClass == "AHUD") {
            HUDClass = "ASandboxHUD";
        }
        AGameModeBase::InitGame();
    }

} // namespace Leon
