#include "ASandboxGameMode.hpp"
#include "ASandboxDemoPickup.hpp"
#include "Engine/UWorld.hpp"
#include "Renderer/FWorldRenderer.hpp"

namespace Leon {

    ASandboxGameMode::ASandboxGameMode(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AGameMode(InHandle, InWorld, InName) {
        SetClass("ASandboxGameMode");
        HUDClass = "ASandboxHUD";
        DefaultPawnClass = "ADefaultPawn";
        PlayerControllerClass = "APlayerController";
        GameStateClass = "AGameState";
        PlayerStateClass = "APlayerState";
    }

    void ASandboxGameMode::InitGame() {
        if (HUDClass.empty() || HUDClass == "AHUD")
            HUDClass = "ASandboxHUD";
        AGameModeBase::InitGame();
    }

    void ASandboxGameMode::StartPlay() {
        if (World && World->GetNetMode() == ENetMode::Client)
            return;
        AGameModeBase::StartPlay();
        StartMatch();
        SetupPlanarReflections();
        SpawnShowcaseDemos();
    }

    void ASandboxGameMode::SetupPlanarReflections() {
        if (!World)
            return;
        auto* renderer = World->GetWorldRenderer();
        if (!renderer)
            return;
        renderer->ClearPlanarReflectionPlanes();
        // n·x + Distance = 0. Showcase floor sits at y = -0.03 to avoid mesh z-fighting.
        renderer->AddPlanarReflectionPlane({0.0f, 1.0f, 0.0f}, 0.03f);
    }

    void ASandboxGameMode::SpawnShowcaseDemos() {
        if (!World)
            return;
        auto* pickup = World->SpawnActor<ASandboxDemoPickup>("SandboxDemoPickup");
        if (pickup)
            pickup->SetActorLocation({0.0f, 0.55f, 7.0f});
    }

} // namespace Leon
