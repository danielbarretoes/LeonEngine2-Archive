#include "ASandboxGameMode.hpp"
#include "ASandboxDemoPickup.hpp"
#include "Engine/Components.hpp"
#include "Engine/UEngine.hpp"
#include "Engine/UWorld.hpp"
#include "Renderer/FWorldRenderer.hpp"

namespace Leon {

    namespace {
        bool CurrentMapContains(const char* InNeedle) {
            if (!UEngine::HasInstance())
                return false;
            return UEngine::Get().GetCurrentMapName().find(InNeedle) != std::string::npos;
        }

        bool IsShowcaseMap() {
            if (!UEngine::HasInstance())
                return true;
            const std::string& map = UEngine::Get().GetCurrentMapName();
            if (map.empty())
                return true;
            return map.find("ShowcaseLevel") != std::string::npos;
        }

        bool IsNightMap() {
            return CurrentMapContains("NightLevel");
        }

        void EnableFloorPlanarOnActor(AActor* InActor) {
            if (!InActor)
                return;
            auto apply = [](FMaterialInstance* InMat) {
                if (!InMat)
                    return;
                InMat->SetUsePlanarReflection(true);
                InMat->SetRoughness(0.18f);
            };
            if (InActor->HasComponent<FMaterialComponent>())
                apply(InActor->GetComponent<FMaterialComponent>().MaterialInstance.get());
            if (InActor->HasComponent<FStaticMeshComponent>()) {
                for (auto& mat : InActor->GetComponent<FStaticMeshComponent>().MaterialOverrides)
                    apply(mat.get());
            }
        }
    } // namespace

    ASandboxGameMode::ASandboxGameMode(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AGameModeBase(InHandle, InWorld, InName) {
        SetClass("ASandboxGameMode");
        HUDClass = "ASandboxHUD";
        DefaultPawnClass = "ADefaultPawn";
        PlayerControllerClass = "APlayerController";
        GameStateClass = "AGameStateBase";
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
        SetupPlanarReflections();
        if (IsShowcaseMap())
            SpawnShowcaseDemos();
        else if (IsNightMap())
            SpawnNightDemos();
    }

    void ASandboxGameMode::SetupPlanarReflections() {
        if (!World)
            return;
        auto* renderer = World->GetWorldRenderer();
        if (!renderer)
            return;
        renderer->ClearPlanarReflectionPlanes();
        renderer->AddPlanarReflectionPlane({0.0f, 1.0f, 0.0f}, 0.0f);
    }

    void ASandboxGameMode::SpawnShowcaseDemos() {
        if (!World)
            return;
        auto* pickup = World->SpawnActor<ASandboxDemoPickup>("SandboxDemoPickup");
        if (pickup)
            pickup->SetActorLocation({0.0f, 0.55f, 7.0f});
    }

    void ASandboxGameMode::SpawnNightDemos() {
        if (!World)
            return;
        // Baked asphalt is too rough for planar; runtime override keeps the lightmap hash intact.
        if (AActor* ground = World->FindActorByName("Ground"))
            EnableFloorPlanarOnActor(ground);
    }

} // namespace Leon
