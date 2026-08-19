#include "ALeonTournamentRenderLabGameMode.hpp"
#include "ALeonTournamentCharacter.hpp"
#include "ALeonTournamentPlayerController.hpp"
#include "ALeonTournamentPlayerState.hpp"
#include "ULeonTournamentGameInstance.hpp"
#include "Assets/UAssetManager.hpp"
#include "Core/FApplication.hpp"
#include "Engine/Components.hpp"
#include "Engine/UEngine.hpp"
#include "Engine/UWorld.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Gameplay/APlayerCameraManager.hpp"
#include "Gameplay/APawn.hpp"
#include "Gameplay/AGameModeBase.hpp"
#include "Renderer/FMeshPrimitives.hpp"
#include "Renderer/FWorldRenderer.hpp"
#include "Renderer/FMaterial.hpp"
#include "Renderer/FMaterialInstance.hpp"

#include <algorithm>

namespace Leon {

    namespace {
        constexpr const char* kLabCameraNames[] = {"Lab Camera", "Lab Camera B", "Lab Camera C"};

        const char* LabCameraActorName(int32_t InIndex) {
            const int32_t clamped = std::clamp(InIndex, 0, ALeonTournamentRenderLabGameMode::LabCameraCount - 1);
            return kLabCameraNames[clamped];
        }

        void MarkActiveLabCameraPrimary(UWorld* InWorld, int32_t InActiveIndex) {
            if (!InWorld)
                return;
            for (int32_t i = 0; i < ALeonTournamentRenderLabGameMode::LabCameraCount; ++i) {
                AActor* actor = InWorld->FindActorByName(kLabCameraNames[i]);
                if (!actor || !actor->HasComponent<FCameraComponent>())
                    continue;
                actor->GetComponent<FCameraComponent>().bPrimary = (i == InActiveIndex);
            }
        }

        void PlaceGrounded(ALeonTournamentCharacter* InCharacter, const glm::vec3& InLocation, float InFloorZ) {
            if (!InCharacter)
                return;
            InCharacter->SetFloorZ(InFloorZ);
            glm::vec3 loc = InLocation;
            loc.y = InFloorZ + InCharacter->GetCapsuleHalfHeight();
            InCharacter->SetActorLocation(loc);
            InCharacter->SnapToFloorPublic();
        }
    } // namespace

    ALeonTournamentRenderLabGameMode::ALeonTournamentRenderLabGameMode(entt::entity InHandle, UWorld* InWorld,
                                                                       const std::string& InName)
        : ALeonTournamentGameMode(InHandle, InWorld, InName) {
        SetClass("ALeonTournamentRenderLabGameMode");
        ApplyLabClasses();
        FLeonTournamentMatchConfig cfg = GetMatchConfig();
        cfg.MaxPlayers = 1;
        cfg.MaxTeamSize = 1;
        cfg.RespawnDelaySeconds = 0.0f;
        cfg.StartCountdownSeconds = 0.0f;
        cfg.MatchDurationSeconds = 0.0f;
        cfg.bFriendlyFire = false;
        SetMatchConfig(cfg);
        SetPreferThirdPerson(true);
    }

    void ALeonTournamentRenderLabGameMode::ApplyLabClasses() {
        DefaultPawnClass = "ALeonTournamentCharacter";
        HUDClass = "ALeonTournamentHUD";
        PlayerControllerClass = "ALeonTournamentPlayerController";
        GameStateClass = "ALeonTournamentGameState";
        PlayerStateClass = "ALeonTournamentPlayerState";
    }

    void ALeonTournamentRenderLabGameMode::InitGame() {
        ApplyLabClasses();
        ALeonTournamentGameMode::InitGame();
    }

    void ALeonTournamentRenderLabGameMode::StartPlay() {
        if (World && World->GetNetMode() == ENetMode::Client)
            return;
        DefaultPawnClass = "ALeonTournamentCharacter";
        AGameModeBase::StartPlay();

        SpawnLabProps();
        EnsurePlanarPlane();
        PunchLabPreview();

        if (auto* gs = GetGameState())
            gs->SetMatchState(ELeonTournamentMatchState::Playing);

        if (auto* pc = World ? World->GetFirstPlayerController() : nullptr) {
            pc->SetInputModeUIOnly();
            pc->SetShowMouseCursor(true);
            if (auto* pawn = pc->GetPawn<ALeonTournamentCharacter>()) {
                IdleCharacter = pawn;
                SetupFixedCamera();
                PlaceGrounded(pawn, {1.6f, 0.0f, -0.6f}, 0.0f);
                pawn->SetActorRotation({0.0f, -35.0f, 0.0f});
                pc->UnPossess();
            } else if (pc->GetPawn()) {
                RestartPlayer(pc);
            } else {
                SpawnIdleCharacter();
            }
            BindLabCamera();
        }
    }

    void ALeonTournamentRenderLabGameMode::Tick(float DeltaSeconds) {
        ALeonTournamentGameMode::Tick(DeltaSeconds);
        if (World && World->GetNetMode() == ENetMode::Client)
            return;
        auto* pc = dynamic_cast<ALeonTournamentPlayerController*>(World ? World->GetFirstPlayerController() : nullptr);
        if (pc && pc->ConsumeEscapePressed())
            ReturnToMenu();
        BindLabCamera();
        PunchLabPreview();
    }

    void ALeonTournamentRenderLabGameMode::RestartPlayer(AController* NewPlayer) {
        AGameModeBase::RestartPlayer(NewPlayer);
        if (auto* pawn = NewPlayer ? NewPlayer->GetPawn<ALeonTournamentCharacter>() : nullptr) {
            IdleCharacter = pawn;
            SetupFixedCamera();
            PlaceGrounded(pawn, {1.6f, 0.0f, -0.6f}, 0.0f);
            pawn->SetActorRotation({0.0f, -35.0f, 0.0f});
            NewPlayer->UnPossess();
        }
        if (auto* pc = dynamic_cast<APlayerController*>(NewPlayer)) {
            pc->SetInputModeUIOnly();
            pc->SetShowMouseCursor(true);
            BindLabCamera();
        }
    }

    void ALeonTournamentRenderLabGameMode::SetupFixedCamera() {
        if (!IdleCharacter)
            return;
        IdleCharacter->SetMenuShowcase(true);
        if (IdleCharacter->HasComponent<FCameraComponent>())
            IdleCharacter->GetComponent<FCameraComponent>().bPrimary = false;
    }

    void ALeonTournamentRenderLabGameMode::BindLabCamera() {
        if (!World)
            return;
        auto* pc = World->GetFirstPlayerController();
        if (!pc)
            return;
        pc->SetInputModeUIOnly();
        pc->SetShowMouseCursor(true);
        if (APawn* pawn = pc->GetPawn()) {
            if (pawn->HasComponent<FCameraComponent>())
                pawn->GetComponent<FCameraComponent>().bPrimary = false;
        }
        APlayerCameraManager* pcm = pc->GetPlayerCameraManager();
        if (pcm && pcm->IsBlendingViewTarget())
            return;
        if (pc->GetViewTarget())
            return;
        if (AActor* labCam = World->FindActorByName(LabCameraActorName(LabCameraIndex))) {
            MarkActiveLabCameraPrimary(World, LabCameraIndex);
            if (pcm)
                pcm->SetViewTarget(labCam);
        }
    }

    void ALeonTournamentRenderLabGameMode::CycleLabCamera() {
        if (!World)
            return;
        auto* pc = World->GetFirstPlayerController();
        if (!pc)
            return;
        LabCameraIndex = (LabCameraIndex + 1) % LabCameraCount;
        AActor* dest = World->FindActorByName(LabCameraActorName(LabCameraIndex));
        if (!dest)
            return;
        MarkActiveLabCameraPrimary(World, LabCameraIndex);
        pc->SetViewTargetWithBlend(dest, 1.15f);
    }

    void ALeonTournamentRenderLabGameMode::PunchLabPreview() {
        if (!World)
            return;
        FWorldRenderer* renderer = World->GetWorldRenderer();
        if (!renderer)
            return;
        auto& pp = renderer->GetPostProcessSettings();
        // Stock bloom (0.05 / threshold 1) and SSAO (radius 0.5) read as no-ops in this small HDR lab.
        if (pp.bBloomEnabled) {
            pp.BloomIntensity = 0.7f;
            pp.BloomThreshold = 0.35f;
        } else {
            pp.BloomIntensity = 0.0f;
            pp.BloomThreshold = 1.2f;
        }
        if (pp.bSSAOEnabled) {
            pp.SSAORadius = 1.5f;
            pp.SSAOIntensity = 2.8f;
            pp.SSAOBias = 0.018f;
        }
    }

    void ALeonTournamentRenderLabGameMode::EnsurePlanarPlane() {
        if (!World)
            return;
        FWorldRenderer* renderer = World->GetWorldRenderer();
        if (!renderer)
            return;
        renderer->ClearPlanarReflectionPlanes();
        renderer->AddPlanarReflectionPlane({0.0f, 1.0f, 0.0f}, 0.0f);
    }

    void ALeonTournamentRenderLabGameMode::SpawnIdleCharacter() {
        if (!World)
            return;
        IdleCharacter = World->SpawnActor<ALeonTournamentCharacter>("RenderLabIdle");
        if (!IdleCharacter)
            return;
        if (auto* gi = UEngine::HasInstance()
                           ? dynamic_cast<ULeonTournamentGameInstance*>(UEngine::Get().GetGameInstance().get())
                           : nullptr) {
            IdleCharacter->ApplyCharacterSkin(gi->GetSelectedCharacterSkin());
        }
        SetupFixedCamera();
        PlaceGrounded(IdleCharacter, {1.6f, 0.0f, -0.6f}, 0.0f);
        IdleCharacter->SetActorRotation({0.0f, -35.0f, 0.0f});
    }

    void ALeonTournamentRenderLabGameMode::SpawnLabProps() {
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

        // Dielectric roughness ramp (left row) — shadow/filter contact visible on floor.
        const float roughnesses[] = {0.04f, 0.2f, 0.5f, 1.0f};
        for (int i = 0; i < 4; ++i) {
            auto mat = parent->CreateInstance("RL_Dielectric" + std::to_string(i));
            mat->SetAlbedoColor({0.72f, 0.72f, 0.75f});
            mat->SetMetallic(0.0f);
            mat->SetRoughness(roughnesses[i]);
            spawnMesh("RL_Dielectric" + std::to_string(i), FMeshPrimitives::CreateSphere(0.35f, 24, 16),
                      {-4.2f + static_cast<float>(i) * 1.05f, 0.45f, 0.2f}, mat, "Sphere");
        }

        // Metal roughness ramp (right-ish row) — specular + shadow penumbra.
        for (int i = 0; i < 4; ++i) {
            auto mat = parent->CreateInstance("RL_Metal" + std::to_string(i));
            mat->SetAlbedoColor({0.95f, 0.78f, 0.42f});
            mat->SetMetallic(1.0f);
            mat->SetRoughness(roughnesses[i]);
            spawnMesh("RL_Metal" + std::to_string(i), FMeshPrimitives::CreateSphere(0.35f, 24, 16),
                      {-4.2f + static_cast<float>(i) * 1.05f, 0.45f, 1.5f}, mat, "Sphere");
        }

        if (auto mirror = UAssetManager::GetMaterialInstance("/Game/Materials/M_ArenaMirror.lmat")) {
            spawnMesh("RL_Mirror", FMeshPrimitives::CreateSphere(0.42f, 28, 18), {-0.2f, 0.52f, -1.5f}, mirror,
                      "Sphere");
        }

        auto plastic = parent->CreateInstance("RL_Plastic");
        plastic->SetAlbedoColor({0.85f, 0.18f, 0.16f});
        plastic->SetMetallic(0.0f);
        plastic->SetRoughness(0.35f);
        spawnMesh("RL_Plastic", FMeshPrimitives::CreateSphere(0.4f, 24, 16), {1.0f, 0.5f, -1.5f}, plastic, "Sphere");

        // SSAO corner: two walls + a sphere in the crease.
        auto wallMat = parent->CreateInstance("RL_SSAOWall");
        wallMat->SetAlbedoColor({0.40f, 0.41f, 0.44f});
        wallMat->SetMetallic(0.0f);
        wallMat->SetRoughness(0.85f);
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
        spawnWall("RL_SSAOWallX", {-6.2f, 1.0f, -3.6f}, {4.0f, 2.0f, 0.2f});
        spawnWall("RL_SSAOWallZ", {-8.1f, 1.0f, -1.7f}, {0.2f, 2.0f, 4.0f});
        spawnMesh("RL_SSAOSphere", FMeshPrimitives::CreateSphere(0.35f, 24, 16), {-6.9f, 0.45f, -2.8f}, wallMat,
                  "Sphere");

        if (auto brick = UAssetManager::GetMaterialInstance("/Game/Materials/M_LabWall.lmat")) {
            spawnMesh("RL_BrickCube", FMeshPrimitives::CreateCube(0.9f), {2.2f, 0.45f, -2.2f}, brick, "Cube");
        }

        if (auto chrome = UAssetManager::GetMaterialInstance("/Game/Materials/M_ArenaMirror.lmat")) {
            spawnMesh("RL_ChromeCube", FMeshPrimitives::CreateCube(0.7f), {3.6f, 0.45f, -2.0f}, chrome, "Cube");
        }

        auto columnMat = parent->CreateInstance("RL_Column");
        columnMat->SetAlbedoColor({0.62f, 0.63f, 0.66f});
        columnMat->SetMetallic(0.15f);
        columnMat->SetRoughness(0.4f);
        spawnMesh("RL_Column", FMeshPrimitives::CreateCylinder(0.18f, 0.18f, 1.6f, 20, true), {0.35f, 0.8f, 2.4f},
                  columnMat, "Cylinder");

        auto glow = parent->CreateInstance("RL_BloomOrb");
        glow->SetAlbedoColor({0.15f, 0.04f, 0.0f});
        glow->SetMetallic(0.0f);
        glow->SetRoughness(0.6f);
        glow->SetEmissiveColor({1.0f, 0.55f, 0.12f});
        glow->SetEmissiveIntensity(12.0f);
        spawnMesh("RL_BloomOrb", FMeshPrimitives::CreateSphere(0.28f, 20, 14), {0.85f, 0.42f, 3.35f}, glow, "Sphere");

        spawnWall("RL_ContactCube", {-0.85f, 0.5f, 3.15f}, {0.4f, 1.0f, 0.4f});

        // Tall pillar for cascade / long-shadow comparison toward the sun.
        auto pillarMat = parent->CreateInstance("RL_Pillar");
        pillarMat->SetAlbedoColor({0.55f, 0.56f, 0.58f});
        pillarMat->SetMetallic(0.05f);
        pillarMat->SetRoughness(0.55f);
        spawnWall("RL_Pillar", {3.4f, 1.4f, -0.4f}, {0.45f, 2.8f, 0.45f});
    }

} // namespace Leon
