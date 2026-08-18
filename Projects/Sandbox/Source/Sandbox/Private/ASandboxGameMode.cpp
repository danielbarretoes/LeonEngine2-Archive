#include "ASandboxGameMode.hpp"
#include "ASandboxDemoPickup.hpp"
#include "Core/FApplication.hpp"
#include "Engine/Components.hpp"
#include "Engine/UEngine.hpp"
#include "Engine/UWorld.hpp"
#include "Assets/UAssetManager.hpp"
#include "Gameplay/AActor.hpp"
#include "Renderer/FMaterial.hpp"
#include "Renderer/FMaterialInstance.hpp"
#include "Renderer/FMeshPrimitives.hpp"
#include "Renderer/FWorldRenderer.hpp"

#include <string>

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

        bool IsRendererLabMap() {
            return CurrentMapContains("RendererLab");
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
        else if (IsRendererLabMap())
            SpawnRendererLab();
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

    void ASandboxGameMode::SpawnRendererLab() {
        if (!World || !FApplication::HasInstance())
            return;

        auto shader = UAssetManager::GetShader("Engine/Assets/Shaders/PBR_Lit.glsl");
        auto parent = UAssetManager::GetDefaultMaterial();
        if (!shader || !parent)
            return;

        auto spawnMesh = [&](const std::string& InName, const TRef<FVertexArray>& InVA, const glm::vec3& InLoc,
                             const TRef<FMaterialInstance>& InMat, const std::string& InMeshType) {
            AActor* actor = World->SpawnActor<AActor>(InName);
            if (!actor || !InVA || !InMat)
                return;
            actor->SetActorLocation(InLoc);
            auto& mesh = actor->AddComponent<FMeshComponent>(InVA, shader);
            mesh.MeshType = InMeshType;
            mesh.Mobility = EComponentMobility::Movable;
            mesh.bCastShadows = true;
            mesh.bReceiveShadows = true;
            actor->AddComponent<FMaterialComponent>(InMat);
        };

        const float roughnesses[] = {0.04f, 0.2f, 0.5f, 1.0f};
        for (int i = 0; i < 4; ++i) {
            auto mat = parent->CreateInstance("LabDielectric" + std::to_string(i));
            mat->SetAlbedoColor({0.7f, 0.7f, 0.72f});
            mat->SetMetallic(0.0f);
            mat->SetRoughness(roughnesses[i]);
            spawnMesh("LabDielectric" + std::to_string(i), FMeshPrimitives::CreateSphere(0.35f, 24, 16),
                      {-4.5f + static_cast<float>(i) * 1.1f, 0.45f, 0.0f}, mat, "Sphere");
        }
        for (int i = 0; i < 4; ++i) {
            auto mat = parent->CreateInstance("LabMetal" + std::to_string(i));
            mat->SetAlbedoColor({0.95f, 0.78f, 0.42f});
            mat->SetMetallic(1.0f);
            mat->SetRoughness(roughnesses[i]);
            spawnMesh("LabMetal" + std::to_string(i), FMeshPrimitives::CreateSphere(0.35f, 24, 16),
                      {-4.5f + static_cast<float>(i) * 1.1f, 0.45f, 1.4f}, mat, "Sphere");
        }

        if (auto chrome = UAssetManager::GetMaterialInstance("/Game/Materials/M_ChromeMirror.lmat")) {
            spawnMesh("LabChrome", FMeshPrimitives::CreateSphere(0.4f, 28, 18), {0.8f, 0.5f, -1.4f}, chrome, "Sphere");
        }

        auto nrmMat = parent->CreateInstance("LabNormalSphere");
        nrmMat->SetAlbedoColor({0.55f, 0.55f, 0.58f});
        nrmMat->SetMetallic(0.0f);
        nrmMat->SetRoughness(0.45f);
        if (auto nrm = UAssetManager::GetTexture2D("/Game/Textures/T_Brick_Normal.ltex"))
            nrmMat->SetTexture(1, nrm);
        spawnMesh("LabNormalSphere", FMeshPrimitives::CreateSphere(0.4f, 24, 16), {2.2f, 0.5f, -1.4f}, nrmMat,
                  "Sphere");

        spawnMesh("LabUvCube", FMeshPrimitives::CreateCube(1.0f), {-2.2f, 0.5f, -1.4f}, parent->CreateInstance("LabCube"),
                  "Cube");

        auto wallMat = parent->CreateInstance("LabSSAOWall");
        wallMat->SetAlbedoColor({0.42f, 0.43f, 0.45f});
        wallMat->SetMetallic(0.0f);
        wallMat->SetRoughness(0.82f);
        auto spawnWall = [&](const std::string& InName, const glm::vec3& InLoc, const glm::vec3& InScale) {
            AActor* actor = World->SpawnActor<AActor>(InName);
            if (!actor)
                return;
            actor->SetActorLocation(InLoc);
            actor->SetActorScale(InScale);
            auto& mesh = actor->AddComponent<FMeshComponent>(FMeshPrimitives::CreateCube(1.0f), shader);
            mesh.MeshType = "Cube";
            mesh.Mobility = EComponentMobility::Movable;
            mesh.bCastShadows = true;
            mesh.bReceiveShadows = true;
            actor->AddComponent<FMaterialComponent>(wallMat);
        };
        spawnWall("LabSSAOWallX", {-6.5f, 1.0f, -4.0f}, {4.0f, 2.0f, 0.2f});
        spawnWall("LabSSAOWallZ", {-8.4f, 1.0f, -2.1f}, {0.2f, 2.0f, 4.0f});
        spawnMesh("LabSSAOSphere", FMeshPrimitives::CreateSphere(0.35f, 24, 16), {-7.2f, 0.45f, -3.2f}, wallMat,
                  "Sphere");

        if (auto glass = UAssetManager::GetMaterialInstance("/Game/Materials/M_GlassTransparent.lmat")) {
            spawnMesh("LabGlass", FMeshPrimitives::CreateQuad(1.6f, 1.8f), {4.0f, 1.0f, -0.5f}, glass, "Quad");
        }
    }

} // namespace Leon
