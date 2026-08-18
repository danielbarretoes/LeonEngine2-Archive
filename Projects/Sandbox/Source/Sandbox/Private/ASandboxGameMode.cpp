#include "ASandboxGameMode.hpp"
#include "ASandboxDemoPickup.hpp"
#include "Assets/UAssetManager.hpp"
#include "Core/FApplication.hpp"
#include "Engine/Components.hpp"
#include "Engine/UEngine.hpp"
#include "Engine/UWorld.hpp"
#include "Gameplay/FProceduralPrimitiveSpawner.hpp"
#include "Renderer/FMeshPrimitives.hpp"
#include "Renderer/FWorldRenderer.hpp"

namespace Leon {

    namespace {
        constexpr float kWallMirrorZ = -6.5f;

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

        AActor* SpawnStaticVisualBox(UWorld* InWorld, const std::string& InName, const glm::vec3& InLocation,
                                     const glm::vec3& InScale, const char* InMaterialPath, bool bCastShadows,
                                     bool bVisibleInReflection) {
            AActor* actor = FProceduralPrimitiveSpawner::SpawnStaticBox(InWorld, InName, InLocation, InScale);
            if (!actor || !FApplication::HasInstance())
                return actor;
            auto va = FMeshPrimitives::CreateCube(1.0f);
            auto shader = UAssetManager::GetShader("Engine/Assets/Shaders/PBR_Lit.glsl");
            if (!va || !shader)
                return actor;
            auto& mesh = actor->AddComponent<FMeshComponent>(va, shader);
            mesh.MeshType = "Cube";
            mesh.MeshSize = 1.0f;
            mesh.Mobility = EComponentMobility::Static;
            mesh.bCastShadows = bCastShadows;
            mesh.bReceiveShadows = true;
            mesh.bVisibleInReflection = bVisibleInReflection;
            if (auto mat = UAssetManager::GetMaterialInstance(InMaterialPath)) {
                actor->AddComponent<FMaterialComponent>(mat, InMaterialPath);
            } else if (auto parent = UAssetManager::GetDefaultMaterial()) {
                auto inst = parent->CreateInstance(InName + "Mat");
                inst->SetAlbedoColor({0.92f, 0.94f, 0.98f});
                inst->SetMetallic(1.0f);
                inst->SetRoughness(0.04f);
                inst->SetUsePlanarReflection(true);
                actor->AddComponent<FMaterialComponent>(inst);
            }
            return actor;
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
        // Flow: dual FBO — floor always; Showcase also registers the facing wall so both captures work.
        renderer->ClearPlanarReflectionPlanes();
        renderer->AddPlanarReflectionPlane({0.0f, 1.0f, 0.0f}, 0.0f);
        if (IsShowcaseMap())
            renderer->AddPlanarReflectionPlane({0.0f, 0.0f, 1.0f}, -kWallMirrorZ);
    }

    void ASandboxGameMode::SpawnShowcaseDemos() {
        if (!World)
            return;
        SpawnStaticVisualBox(World, "SandboxWallMirror", {0.0f, 1.85f, kWallMirrorZ}, {12.0f, 3.6f, 0.05f},
                             "/Game/Materials/M_ChromeMirror.lmat", true, true);

        auto* pickup = World->SpawnActor<ASandboxDemoPickup>("SandboxDemoPickup");
        if (pickup)
            pickup->SetActorLocation({0.0f, 0.55f, 7.0f});
    }

    void ASandboxGameMode::SpawnNightDemos() {
        if (!World)
            return;
        AActor* puddle = SpawnStaticVisualBox(World, "SandboxWetPuddle", {0.0f, 0.03f, 8.0f}, {7.0f, 0.05f, 5.0f},
                                              "/Game/Materials/M_ChromeMirror.lmat", false, false);
        if (puddle && puddle->HasComponent<FMaterialComponent>()) {
            if (auto mat = puddle->GetComponent<FMaterialComponent>().MaterialInstance) {
                mat->SetAlbedoColor({0.10f, 0.11f, 0.12f});
                mat->SetMetallic(0.82f);
                mat->SetRoughness(0.12f);
                mat->SetUsePlanarReflection(true);
            }
        }
    }

} // namespace Leon
