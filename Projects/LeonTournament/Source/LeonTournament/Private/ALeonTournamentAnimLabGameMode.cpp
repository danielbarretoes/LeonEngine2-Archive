#include "ALeonTournamentAnimLabGameMode.hpp"
#include "ALeonTournamentDummy.hpp"
#include "ALeonTournamentCharacter.hpp"
#include "ALeonTournamentPlayerController.hpp"
#include "ALeonTournamentPlayerState.hpp"
#include "Assets/UAssetManager.hpp"
#include "Core/FApplication.hpp"
#include "Core/FInput.hpp"
#include "Engine/Components.hpp"
#include "Engine/UWorld.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Gameplay/APlayerStart.hpp"
#include "Gameplay/UPrimitiveComponent.hpp"
#include "Renderer/FMeshPrimitives.hpp"

namespace Leon {

    namespace {
        AActor* SpawnLabBox(UWorld* InWorld, const std::string& InName, const glm::vec3& InLocation,
                            const glm::vec3& InScale, const glm::vec3& InColor) {
            if (!InWorld)
                return nullptr;
            AActor* actor = InWorld->SpawnActor<AActor>(InName);
            actor->SetActorLocation(InLocation);
            actor->SetActorScale(InScale);
            auto box = actor->AddActorComponent<UBoxComponent>("Box");
            box->SetBoxExtent(glm::vec3(0.5f));
            box->SetCollisionObjectType(ECollisionChannel::WorldStatic);
            box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            if (FApplication::HasInstance()) {
                auto va = FMeshPrimitives::CreateCube(1.0f);
                auto shader = UAssetManager::GetShader("Engine/Assets/Shaders/PBR_Lit.glsl");
                if (va && shader) {
                    auto& mesh = actor->AddComponent<FMeshComponent>(va, shader);
                    mesh.MeshType = "Cube";
                    mesh.MeshSize = 1.0f;
                    mesh.Mobility = EComponentMobility::Static;
                    if (auto parent = UAssetManager::GetDefaultMaterial()) {
                        auto inst = parent->CreateInstance(InName + "Mat");
                        inst->SetAlbedoColor(InColor);
                        actor->AddComponent<FMaterialComponent>(inst);
                    }
                }
            }
            return actor;
        }
    } // namespace

    ALeonTournamentAnimLabGameMode::ALeonTournamentAnimLabGameMode(entt::entity InHandle, UWorld* InWorld,
                                                                   const std::string& InName)
        : ALeonTournamentGameMode(InHandle, InWorld, InName) {
        SetClass("ALeonTournamentAnimLabGameMode");
        ApplyLabClasses();
        FLeonTournamentMatchConfig cfg = GetMatchConfig();
        cfg.MaxPlayers = 1;
        cfg.MaxTeamSize = 1;
        cfg.RespawnDelaySeconds = 4.0f;
        cfg.StartCountdownSeconds = 0.0f;
        SetMatchConfig(cfg);
    }

    void ALeonTournamentAnimLabGameMode::ApplyLabClasses() {
        DefaultPawnClass = "ALeonTournamentCharacter";
        HUDClass = "ALeonTournamentHUD";
        PlayerControllerClass = "ALeonTournamentPlayerController";
        GameStateClass = "ALeonTournamentGameState";
        PlayerStateClass = "ALeonTournamentPlayerState";
    }

    void ALeonTournamentAnimLabGameMode::InitGame() {
        ApplyLabClasses();
        ALeonTournamentGameMode::InitGame();
    }

    void ALeonTournamentAnimLabGameMode::ApplyCameraPreference(ALeonTournamentCharacter* InCharacter) {
        if (!InCharacter)
            return;
        InCharacter->SetThirdPerson(bPreferThirdPerson);
        InCharacter->SetFloorZ(0.0f);
    }

    void ALeonTournamentAnimLabGameMode::HandleCameraToggle() {
        const bool bDown = FInput::IsKeyPressed(Key::V);
        const bool bEdge = bDown && !bCameraToggleWasDown;
        bCameraToggleWasDown = bDown;
        if (!bEdge)
            return;
        bPreferThirdPerson = !bPreferThirdPerson;
        if (auto* pc = World ? World->GetFirstPlayerController() : nullptr)
            ApplyCameraPreference(pc->GetPawn<ALeonTournamentCharacter>());
    }

    void ALeonTournamentAnimLabGameMode::TickDummyRespawn(float DeltaSeconds) {
        if (!Dummy || Dummy->IsPendingKill()) {
            DummyRespawnRemaining = -1.0f;
            return;
        }
        if (!Dummy->IsDeadFrozen()) {
            DummyRespawnRemaining = -1.0f;
            return;
        }
        if (DummyRespawnRemaining < 0.0f)
            DummyRespawnRemaining = GetMatchConfig().RespawnDelaySeconds;
        DummyRespawnRemaining -= DeltaSeconds;
        if (DummyRespawnRemaining <= 0.0f) {
            DummyRespawnRemaining = -1.0f;
            RespawnDummy();
        }
    }

    void ALeonTournamentAnimLabGameMode::StartPlay() {
        if (World && World->GetNetMode() == ENetMode::Client)
            return;
        AGameModeBase::StartPlay();
        BuildAnimLab();
        EnsurePlayableLighting();
        if (auto* gs = GetGameState())
            gs->SetMatchState(ELeonTournamentMatchState::Playing);
        if (auto* pc = World ? World->GetFirstPlayerController() : nullptr) {
            pc->SetInputModeGameOnly();
            ApplyCameraPreference(pc->GetPawn<ALeonTournamentCharacter>());
            if (auto* ps = dynamic_cast<ALeonTournamentPlayerState*>(pc->GetPlayerState())) {
                if (ps->GetTeam() == ELeonTournamentTeam::None)
                    ps->SetTeam(ELeonTournamentTeam::Team1);
            }
        }
        SpawnDummy();
    }

    void ALeonTournamentAnimLabGameMode::Tick(float DeltaSeconds) {
        ALeonTournamentGameMode::Tick(DeltaSeconds);
        if (World && World->GetNetMode() == ENetMode::Client)
            return;
        auto* pc = dynamic_cast<ALeonTournamentPlayerController*>(World ? World->GetFirstPlayerController() : nullptr);
        if (pc && pc->ConsumeEscapePressed())
            ReturnToMenu();
        HandleCameraToggle();
        TickDummyRespawn(DeltaSeconds);
    }

    void ALeonTournamentAnimLabGameMode::RestartPlayer(AController* NewPlayer) {
        AGameModeBase::RestartPlayer(NewPlayer);
        ApplyCameraPreference(NewPlayer ? NewPlayer->GetPawn<ALeonTournamentCharacter>() : nullptr);
    }

    void ALeonTournamentAnimLabGameMode::BuildAnimLab() {
        if (!World)
            return;
        // Map actors (LabFloor + walls) provide textured/bakeable geometry — skip duplicates.
        if (World->FindActorByName("LabFloor"))
            return;
        SpawnLabBox(World, "LabFloor", {0.0f, -0.25f, 0.0f}, {32.0f, 0.5f, 32.0f}, {0.22f, 0.24f, 0.26f});
        SpawnLabBox(World, "LabWallN", {0.0f, 1.5f, -16.0f}, {32.0f, 3.0f, 0.5f}, {0.18f, 0.2f, 0.22f});
        SpawnLabBox(World, "LabWallS", {0.0f, 1.5f, 16.0f}, {32.0f, 3.0f, 0.5f}, {0.18f, 0.2f, 0.22f});
        SpawnLabBox(World, "LabWallW", {-16.0f, 1.5f, 0.0f}, {0.5f, 3.0f, 32.0f}, {0.18f, 0.2f, 0.22f});
        SpawnLabBox(World, "LabWallE", {16.0f, 1.5f, 0.0f}, {0.5f, 3.0f, 32.0f}, {0.18f, 0.2f, 0.22f});
        SpawnLabBox(World, "JumpPadLow", {6.0f, 0.6f, 6.0f}, {4.0f, 1.2f, 4.0f}, {0.32f, 0.38f, 0.48f});
        SpawnLabBox(World, "JumpPadHigh", {6.0f, 1.8f, 12.0f}, {3.0f, 0.4f, 3.0f}, {0.42f, 0.32f, 0.22f});
        SpawnLabBox(World, "FallLedge", {-8.0f, 1.2f, 8.0f}, {3.5f, 2.4f, 2.0f}, {0.28f, 0.30f, 0.34f});
    }

    void ALeonTournamentAnimLabGameMode::SpawnDummy() {
        if (!World || Dummy)
            return;
        glm::vec3 loc{5.0f, 2.0f, 0.0f};
        if (AActor* start = FindPlayerStart("Dummy"))
            loc = start->GetActorLocation();
        Dummy = World->SpawnActor<ALeonTournamentDummy>("Dummy");
        Dummy->SetActorLocation(loc);
        Dummy->SetFloorZ(0.0f);
        Dummy->SetThirdPerson(true);
        auto* ps = World->SpawnActor<ALeonTournamentPlayerState>("DummyPS");
        ps->SetPlayerName("Dummy");
        ps->SetTeam(ELeonTournamentTeam::Team2);
        Dummy->SetPlayerState(ps);
    }

    void ALeonTournamentAnimLabGameMode::RespawnDummy() {
        if (!Dummy || !World)
            return;
        glm::vec3 loc{5.0f, 2.0f, 0.0f};
        if (AActor* start = FindPlayerStart("Dummy"))
            loc = start->GetActorLocation();
        Dummy->OnServerRespawn(loc);
        Dummy->SetThirdPerson(true);
        Dummy->SetFloorZ(0.0f);
    }

} // namespace Leon
