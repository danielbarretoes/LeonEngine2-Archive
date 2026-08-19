#include "ALeonTournamentAnimLabGameMode.hpp"
#include "ALeonTournamentDummy.hpp"
#include "ALeonTournamentCharacter.hpp"
#include "ALeonTournamentPickup.hpp"
#include "ALeonTournamentPlayerController.hpp"
#include "ALeonTournamentPlayerState.hpp"
#include "FLeonTournamentArenaBuilder.hpp"
#include "Assets/UAssetManager.hpp"
#include "Core/FApplication.hpp"
#include "Engine/Components.hpp"
#include "Engine/UWorld.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Gameplay/APlayerStart.hpp"
#include "Gameplay/UPrimitiveComponent.hpp"
#include "Renderer/FMeshPrimitives.hpp"

#include <cmath>

namespace Leon {

    namespace {
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
        cfg.MatchDurationSeconds = 0.0f;
        // Lab: always allow hits on the dummy even if lobby team state leaked.
        cfg.bFriendlyFire = true;
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

    void ALeonTournamentAnimLabGameMode::ForceLabTeams() {
        if (auto* pc = World ? World->GetFirstPlayerController() : nullptr) {
            if (auto* ps = dynamic_cast<ALeonTournamentPlayerState*>(pc->GetPlayerState())) {
                ps->SetTeam(ELeonTournamentTeam::Team1);
                ps->SetIsBot(false);
            }
        }
        if (Dummy) {
            if (auto* dps = Dummy->GetPlayerState()) {
                dps->SetTeam(ELeonTournamentTeam::Team2);
                dps->SetIsBot(true);
            }
            Dummy->SetBotControlled(true);
        }
    }

    void ALeonTournamentAnimLabGameMode::StartPlay() {
        if (World && World->GetNetMode() == ENetMode::Client)
            return;
        DefaultPawnClass = "ALeonTournamentCharacter";
        AGameModeBase::StartPlay();
        BuildAnimLab();
        EnsureLabColliders();
        SpawnLabWeaponPickups();
        if (auto* gs = GetGameState())
            gs->SetMatchState(ELeonTournamentMatchState::Playing);
        if (auto* pc = World ? World->GetFirstPlayerController() : nullptr) {
            pc->SetInputModeGameOnly();
            if (auto* pawn = pc->GetPawn<ALeonTournamentCharacter>()) {
                ApplyCameraPreference(pawn);
                PlaceGrounded(pawn, pawn->GetActorLocation(), 0.0f);
            } else if (pc->GetPawn()) {
                RestartPlayer(pc);
            }
        }
        SpawnDummy();
        ForceLabTeams();
    }

    void ALeonTournamentAnimLabGameMode::Tick(float DeltaSeconds) {
        ALeonTournamentGameMode::Tick(DeltaSeconds);
        if (World && World->GetNetMode() == ENetMode::Client)
            return;
        auto* pc = dynamic_cast<ALeonTournamentPlayerController*>(World ? World->GetFirstPlayerController() : nullptr);
        if (pc && pc->ConsumeEscapePressed())
            ReturnToMenu();
        TickDummyRespawn(DeltaSeconds);
    }

    void ALeonTournamentAnimLabGameMode::RestartPlayer(AController* NewPlayer) {
        AGameModeBase::RestartPlayer(NewPlayer);
        ApplyCameraPreference(NewPlayer ? NewPlayer->GetPawn<ALeonTournamentCharacter>() : nullptr);
        if (auto* pawn = NewPlayer ? NewPlayer->GetPawn<ALeonTournamentCharacter>() : nullptr)
            PlaceGrounded(pawn, pawn->GetActorLocation(), 0.0f);
        ForceLabTeams();
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

    void ALeonTournamentAnimLabGameMode::EnsureLabColliders() {
        if (!World)
            return;
        auto ensureBox = [](AActor* actor, const glm::vec3& extent) {
            if (!actor || actor->FindActorComponent<UBoxComponent>())
                return;
            auto box = actor->AddActorComponent<UBoxComponent>("Box");
            box->SetBoxExtent(extent);
            box->SetCollisionObjectType(ECollisionChannel::WorldStatic);
            box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        };
        if (AActor* floor = World->FindActorByName("LabFloor")) {
            const auto& scale = floor->GetActorScale();
            if (std::abs(scale.x - 1.0f) < 0.01f && std::abs(scale.z - 1.0f) < 0.01f)
                ensureBox(floor, {16.0f, 0.25f, 16.0f});
            else
                ensureBox(floor, {0.5f, 0.5f, 0.5f});
        }
        for (const char* name : {"LabWallN", "LabWallS", "LabWallW", "LabWallE", "JumpPadLow", "JumpPadHigh",
                                 "FallLedge"}) {
            ensureBox(World->FindActorByName(name), {0.5f, 0.5f, 0.5f});
        }
    }

    void ALeonTournamentAnimLabGameMode::BuildAnimLab() {
        if (!World)
            return;
        // Map actors (LabFloor + walls) provide textured/bakeable geometry — skip duplicates.
        if (World->FindActorByName("LabFloor"))
            return;
        FLeonTournamentArenaBuilder::SpawnSimpleBox(World, "LabFloor", {0.0f, -0.25f, 0.0f}, {32.0f, 0.5f, 32.0f}, {0.22f, 0.24f, 0.26f});
        FLeonTournamentArenaBuilder::SpawnSimpleBox(World, "LabWallN", {0.0f, 1.5f, -16.0f}, {32.0f, 3.0f, 0.5f}, {0.18f, 0.2f, 0.22f});
        FLeonTournamentArenaBuilder::SpawnSimpleBox(World, "LabWallS", {0.0f, 1.5f, 16.0f}, {32.0f, 3.0f, 0.5f}, {0.18f, 0.2f, 0.22f});
        FLeonTournamentArenaBuilder::SpawnSimpleBox(World, "LabWallW", {-16.0f, 1.5f, 0.0f}, {0.5f, 3.0f, 32.0f}, {0.18f, 0.2f, 0.22f});
        FLeonTournamentArenaBuilder::SpawnSimpleBox(World, "LabWallE", {16.0f, 1.5f, 0.0f}, {0.5f, 3.0f, 32.0f}, {0.18f, 0.2f, 0.22f});
        FLeonTournamentArenaBuilder::SpawnSimpleBox(World, "JumpPadLow", {6.0f, 0.6f, 6.0f}, {4.0f, 1.2f, 4.0f}, {0.32f, 0.38f, 0.48f});
        FLeonTournamentArenaBuilder::SpawnSimpleBox(World, "JumpPadHigh", {6.0f, 1.8f, 12.0f}, {3.0f, 0.4f, 3.0f}, {0.42f, 0.32f, 0.22f});
        FLeonTournamentArenaBuilder::SpawnSimpleBox(World, "FallLedge", {-8.0f, 1.2f, 8.0f}, {3.5f, 2.4f, 2.0f}, {0.28f, 0.30f, 0.34f});
    }

    void ALeonTournamentAnimLabGameMode::SpawnLabWeaponPickups() {
        if (!World)
            return;
        constexpr float kLabRespawn = 5.0f;
        auto spawnWeapon = [&](const char* name, ELeonTournamentWeaponId id, const glm::vec3& loc) {
            auto* p = World->SpawnActor<ALeonTournamentWeaponPickup>(name);
            if (!p)
                return;
            p->SetWeaponId(id);
            p->SetRespawnDelay(kLabRespawn);
            p->SetAnchorLocation(loc);
        };
        // One of each arsenal weapon in a short arc for quick lab testing.
        spawnWeapon("LabPU_Rifle", ELeonTournamentWeaponId::Rifle, {-4.0f, 1.0f, 3.0f});
        spawnWeapon("LabPU_Shotgun", ELeonTournamentWeaponId::Shotgun, {-2.0f, 1.0f, 4.0f});
        spawnWeapon("LabPU_Rocket", ELeonTournamentWeaponId::Rocket, {0.0f, 1.0f, 4.5f});
        spawnWeapon("LabPU_Laser", ELeonTournamentWeaponId::Laser, {2.0f, 1.0f, 4.0f});
        spawnWeapon("LabPU_Grenade", ELeonTournamentWeaponId::Grenade, {4.0f, 1.0f, 3.0f});
        spawnWeapon("LabPU_Flamer", ELeonTournamentWeaponId::Flamethrower, {5.0f, 1.0f, 1.5f});
    }

    void ALeonTournamentAnimLabGameMode::SpawnDummy() {
        if (!World || Dummy)
            return;
        glm::vec3 loc{5.0f, 2.0f, 0.0f};
        if (AActor* start = FindPlayerStart(nullptr, "Dummy"))
            loc = start->GetActorLocation();
        Dummy = World->SpawnActor<ALeonTournamentDummy>("Dummy");
        auto* ps = World->SpawnActor<ALeonTournamentPlayerState>("DummyPS");
        ps->SetPlayerName("Dummy");
        ps->SetTeam(ELeonTournamentTeam::Team2);
        ps->SetIsBot(true);
        Dummy->SetPlayerState(ps);
        Dummy->SetBotControlled(true);
        Dummy->SetThirdPerson(true);
        PlaceGrounded(Dummy, loc, 0.0f);
    }

    void ALeonTournamentAnimLabGameMode::RespawnDummy() {
        if (!Dummy || !World)
            return;
        glm::vec3 loc{5.0f, 2.0f, 0.0f};
        if (AActor* start = FindPlayerStart(nullptr, "Dummy"))
            loc = start->GetActorLocation();
        Dummy->OnServerRespawn(loc);
        Dummy->SetThirdPerson(true);
        PlaceGrounded(Dummy, loc, 0.0f);
        ForceLabTeams();
    }

} // namespace Leon
