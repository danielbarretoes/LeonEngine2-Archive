#include "AShooterGameMode.hpp"
#include "AShooterBotController.hpp"
#include "AShooterPlayerController.hpp"
#include "UShooterGameInstance.hpp"
#include "Assets/UAssetManager.hpp"
#include "Core/FApplication.hpp"
#include "Gameplay/UPrimitiveComponent.hpp"
#include "Gameplay/AAIController.hpp"
#include "Gameplay/APlayerStart.hpp"
#include "Gameplay/ANavMeshBoundsVolume.hpp"
#include "Gameplay/ACharacter.hpp"
#include "AI/UNavigationSystem.hpp"
#include "Engine/Components.hpp"
#include "Engine/UEngine.hpp"
#include "Engine/UWorld.hpp"
#include "Core/FLog.hpp"
#include "Gameplay/UGameplayStatics.hpp"
#include "Renderer/FMeshPrimitives.hpp"
#include "Core/FWorldUnits.hpp"

#include <algorithm>
#include <cstdio>

namespace Leon {

    namespace {
        UShooterGameInstance* GetShooterGI() {
            if (!UEngine::HasInstance())
                return nullptr;
            return dynamic_cast<UShooterGameInstance*>(UEngine::Get().GetGameInstance().get());
        }

        AActor* SpawnCollisionBox(UWorld* InWorld, const std::string& InName, const glm::vec3& InLocation,
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

    AShooterGameMode::AShooterGameMode(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AGameModeBase(InHandle, InWorld, InName) {
        SetClass("AShooterGameMode");
        DefaultPawnClass = "None";
        PlayerControllerClass = "AShooterPlayerController";
        HUDClass = "AShooterHUD";
        GameStateClass = "AShooterGameState";
        PlayerStateClass = "AShooterPlayerState";
    }

    AShooterGameState* AShooterGameMode::GetShooterGameState() const {
        return dynamic_cast<AShooterGameState*>(GameState);
    }

    void AShooterGameMode::InitGame() {
        DefaultPawnClass = "None";
        PlayerControllerClass = "AShooterPlayerController";
        HUDClass = "AShooterHUD";
        GameStateClass = "AShooterGameState";
        PlayerStateClass = "AShooterPlayerState";
        AGameModeBase::InitGame();
        if (auto* gs = GetShooterGameState()) {
            gs->SetMatchState(EShooterMatchState::MainMenu);
            gs->SetRemainingTime(Config.MatchDurationSeconds);
        }
    }

    void AShooterGameMode::StartPlay() {
        if (World && World->GetNetMode() == ENetMode::Client) {
            AGameModeBase::StartPlay();
            EnterMainMenu();
            return;
        }
        AGameModeBase::StartPlay();
        EnterMainMenu();
    }

    APlayerController* AShooterGameMode::Login(const std::string& InPlayerName) {
        APlayerController* pc = AGameModeBase::Login(InPlayerName);
        if (auto* ps = pc ? dynamic_cast<AShooterPlayerState*>(pc->GetPlayerState()) : nullptr) {
            if (ps->GetTeam() == EShooterTeam::None)
                ps->SetTeam(AssignTeam());
            ps->SetIsBot(false);
        }
        RefreshTeamCounts();
        return pc;
    }

    void AShooterGameMode::EnterMainMenu() {
        if (auto* gs = GetShooterGameState()) {
            gs->SetMatchState(EShooterMatchState::MainMenu);
            gs->SetMatchWinner(EShooterMatchWinner::None);
            gs->SetTeam1Kills(0);
            gs->SetTeam2Kills(0);
        }
        if (auto* pc = World ? World->GetFirstPlayerController() : nullptr)
            pc->SetInputModeUIOnly();
    }

    void AShooterGameMode::EnterLobby() {
        if (auto* gs = GetShooterGameState())
            gs->SetMatchState(EShooterMatchState::Lobby);
        if (auto* pc = World ? World->GetFirstPlayerController() : nullptr) {
            if (auto* ps = dynamic_cast<AShooterPlayerState*>(pc->GetPlayerState())) {
                if (ps->GetTeam() == EShooterTeam::None)
                    ps->SetTeam(AssignTeam());
                ps->SetIsBot(false);
            }
            pc->SetInputModeUIOnly();
        }
        if (ShouldFillBotsOnEnterLobby())
            FillBotsToCapacity();
        RefreshTeamCounts();
    }

    bool AShooterGameMode::ShouldFillBotsOnEnterLobby() const {
        if (World && World->GetNetMode() == ENetMode::Client)
            return false;
        if (auto* gi = GetShooterGI())
            return gi->GetSessionMode() != EShooterSessionMode::LanHost;
        return true;
    }

    bool AShooterGameMode::IsCombatAllowed() const {
        auto* gs = GetShooterGameState();
        return gs && gs->GetMatchState() == EShooterMatchState::Playing;
    }

    EShooterTeam AShooterGameMode::AssignTeam() {
        const int32_t t1 = CountTeam(EShooterTeam::Team1);
        const int32_t t2 = CountTeam(EShooterTeam::Team2);
        if (t1 <= t2 && t1 < Config.MaxTeamSize)
            return EShooterTeam::Team1;
        if (t2 < Config.MaxTeamSize)
            return EShooterTeam::Team2;
        return EShooterTeam::None;
    }

    int32_t AShooterGameMode::CountTeam(EShooterTeam InTeam) const {
        int32_t n = 0;
        if (!GameState)
            return 0;
        for (APlayerState* ps : GameState->GetPlayerArray()) {
            auto* sps = dynamic_cast<AShooterPlayerState*>(ps);
            if (sps && sps->GetTeam() == InTeam)
                ++n;
        }
        return n;
    }

    void AShooterGameMode::RefreshTeamCounts() {
        if (auto* gs = GetShooterGameState()) {
            gs->SetTeam1PlayerCount(CountTeam(EShooterTeam::Team1));
            gs->SetTeam2PlayerCount(CountTeam(EShooterTeam::Team2));
        }
    }

    AShooterBotController* AShooterGameMode::SpawnBot(EShooterTeam InTeam, const std::string& InName) {
        if (!World)
            return nullptr;
        auto* bot = World->SpawnActor<AShooterBotController>(InName + "PC");
        auto* ps = World->SpawnActor<AShooterPlayerState>(InName + "PS");
        ps->SetPlayerName(InName);
        ps->SetPlayerId(NextPlayerId++);
        ps->SetTeam(InTeam);
        ps->SetIsBot(true);
        bot->SetPlayerState(ps);
        if (GameState)
            GameState->AddPlayerState(ps);
        World->AddAIController(bot);
        bot->SetWaypoints(Waypoints);
        bot->SetCoverPoints(CoverPoints);
        return bot;
    }

    void AShooterGameMode::FillBotsToCapacity() {
        while (CountTeam(EShooterTeam::Team1) + CountTeam(EShooterTeam::Team2) < Config.MaxPlayers) {
            EShooterTeam team = AssignTeam();
            if (team == EShooterTeam::None)
                break;
            char name[32];
            std::snprintf(name, sizeof(name), "Bot_%d", NextBotId++);
            SpawnBot(team, name);
        }
        RefreshTeamCounts();
    }

    void AShooterGameMode::BuildArena() {
        if (bArenaBuilt || !World)
            return;
        bArenaBuilt = true;

        SpawnCollisionBox(World, "Floor", {0.0f, -0.25f, 0.0f}, {48.0f, 0.5f, 48.0f}, {0.22f, 0.24f, 0.26f});
        SpawnCollisionBox(World, "WallN", {0.0f, 1.5f, -24.0f}, {48.0f, 3.0f, 0.6f}, {0.18f, 0.2f, 0.22f});
        SpawnCollisionBox(World, "WallS", {0.0f, 1.5f, 24.0f}, {48.0f, 3.0f, 0.6f}, {0.18f, 0.2f, 0.22f});
        SpawnCollisionBox(World, "WallW", {-24.0f, 1.5f, 0.0f}, {0.6f, 3.0f, 48.0f}, {0.18f, 0.2f, 0.22f});
        SpawnCollisionBox(World, "WallE", {24.0f, 1.5f, 0.0f}, {0.6f, 3.0f, 48.0f}, {0.18f, 0.2f, 0.22f});
        SpawnCollisionBox(World, "CoverA", {-6.0f, 0.7f, 0.0f}, {3.0f, 1.4f, 1.2f}, {0.35f, 0.22f, 0.18f});
        SpawnCollisionBox(World, "CoverB", {6.0f, 0.7f, 0.0f}, {3.0f, 1.4f, 1.2f}, {0.18f, 0.28f, 0.40f});
        SpawnCollisionBox(World, "CoverC", {0.0f, 0.7f, -8.0f}, {1.4f, 1.4f, 4.0f}, {0.30f, 0.30f, 0.20f});
        SpawnCollisionBox(World, "CoverD", {0.0f, 0.7f, 8.0f}, {1.4f, 1.4f, 4.0f}, {0.30f, 0.30f, 0.20f});
        SpawnCollisionBox(World, "RampL", {-12.0f, 0.5f, 6.0f}, {4.0f, 1.0f, 2.0f}, {0.28f, 0.28f, 0.30f});
        SpawnCollisionBox(World, "RampR", {12.0f, 0.5f, -6.0f}, {4.0f, 1.0f, 2.0f}, {0.28f, 0.28f, 0.30f});
        SpawnCollisionBox(World, "MidBlock", {0.0f, 0.6f, 0.0f}, {2.2f, 1.2f, 2.2f}, {0.40f, 0.35f, 0.22f});

        SpawnCollisionBox(World, "ScaleCube1m", {0.0f, 0.5f, -20.0f}, {1.0f, 1.0f, 1.0f}, {0.9f, 0.2f, 0.2f});
        SpawnCollisionBox(World, "ScaleCube10cm", {1.2f, 0.05f, -20.0f}, {0.1f, 0.1f, 0.1f}, {0.2f, 0.9f, 0.2f});

        Team1Spawns = {{-18.0f, 2.0f, -8.0f},  {-18.0f, 2.0f, -2.0f}, {-18.0f, 2.0f, 4.0f}, {-18.0f, 2.0f, 10.0f},
                       {-14.0f, 2.0f, -10.0f}, {-14.0f, 2.0f, 8.0f},  {-20.0f, 2.0f, 0.0f}, {-16.0f, 2.0f, 12.0f}};
        Team2Spawns = {{18.0f, 2.0f, 8.0f},  {18.0f, 2.0f, 2.0f},  {18.0f, 2.0f, -4.0f}, {18.0f, 2.0f, -10.0f},
                       {14.0f, 2.0f, 10.0f}, {14.0f, 2.0f, -8.0f}, {20.0f, 2.0f, 0.0f},  {16.0f, 2.0f, -12.0f}};
        Waypoints = {{-10, 2, -10}, {-10, 2, 10}, {10, 2, -10}, {10, 2, 10}, {0, 2, -14}, {0, 2, 14},
                     {-14, 2, 0},   {14, 2, 0},   {-6, 2, 4},   {6, 2, -4},  {0, 2, 0}};
        CoverPoints = {{-8.0f, 2.0f, 0.0f}, {-4.0f, 2.0f, 0.0f}, {8.0f, 2.0f, 0.0f},  {4.0f, 2.0f, 0.0f},
                       {0.0f, 2.0f, -10.0f}, {0.0f, 2.0f, -6.0f}, {0.0f, 2.0f, 10.0f}, {0.0f, 2.0f, 6.0f},
                       {-12.0f, 2.0f, 8.0f}, {12.0f, 2.0f, -8.0f}};

        for (size_t i = 0; i < Team1Spawns.size(); ++i) {
            auto* start = World->SpawnActor<APlayerStart>("Team1Start_" + std::to_string(i));
            start->SetActorLocation(Team1Spawns[i]);
            start->SetTeamIndex(1);
            start->SetPlayerStartTag("Team1");
        }
        for (size_t i = 0; i < Team2Spawns.size(); ++i) {
            auto* start = World->SpawnActor<APlayerStart>("Team2Start_" + std::to_string(i));
            start->SetActorLocation(Team2Spawns[i]);
            start->SetTeamIndex(2);
            start->SetPlayerStartTag("Team2");
        }

        auto* navBounds = World->SpawnActor<ANavMeshBoundsVolume>("NavBounds");
        navBounds->SetActorLocation({0.0f, 1.0f, 0.0f});
        navBounds->SetActorScale({46.0f, 2.0f, 46.0f});
        World->RebuildNavigation();
        if (auto* nav = World->GetNavigationSystem()) {
            LE_CORE_INFO("Arena nav: built={0} walkable={1} {2}x{3}", nav->IsBuilt() ? 1 : 0, nav->GetWalkableCount(),
                         nav->GetWidth(), nav->GetDepth());
        }

        EnsurePlayableLighting();
    }

    void AShooterGameMode::EnsurePlayableLighting() {
        if (!World)
            return;
        AActor* env = World->FindActorByName("Environment Skybox");
        if (!env)
            env = World->SpawnActor<AActor>("Environment Skybox");
        FSkyboxComponent sky;
        sky.bEnabled = true;
        sky.Exposure = 1.0f;
        sky.SunIntensity = 3.5f;
        sky.EnvironmentIntensity = 1.15f;
        sky.bUseHDREnvironmentMap = true;
        sky.HDREnvironmentMapPath = "/Game/HDR/DaySky1k.lhdr";
        if (FApplication::HasInstance())
            sky.HDREnvironmentMap = UAssetManager::GetTexture2D(sky.HDREnvironmentMapPath);
        sky.SkyZenithColor = {0.18f, 0.44f, 0.88f};
        sky.HorizonColor = {0.78f, 0.84f, 0.95f};
        sky.GroundColor = {0.22f, 0.24f, 0.28f};
        sky.SunColor = {1.0f, 0.98f, 0.92f};
        if (env->HasComponent<FSkyboxComponent>())
            env->GetComponent<FSkyboxComponent>() = sky;
        else
            env->AddComponent<FSkyboxComponent>(sky);

        bool bHasSun = false;
        for (const auto& actor : World->GetAllActors()) {
            if (actor && actor->HasComponent<UDirectionalLightComponent>()) {
                auto& sun = actor->GetComponent<UDirectionalLightComponent>();
                sun.bEnabled = true;
                sun.Mobility = ELightMobility::Stationary;
                sun.Light.Direction = glm::normalize(glm::vec3(-0.35f, -1.0f, -0.25f));
                sun.Light.Color = {1.0f, 0.97f, 0.90f};
                sun.Light.Intensity = 3.5f;
                bHasSun = true;
                break;
            }
        }
        if (!bHasSun) {
            AActor* sunActor = World->SpawnActor<AActor>("Directional Sunlight");
            sunActor->SetActorLocation({0.0f, 12.0f, 0.0f});
            UDirectionalLightComponent sun;
            sun.bEnabled = true;
            sun.Mobility = ELightMobility::Stationary;
            sun.Light.Direction = glm::normalize(glm::vec3(-0.35f, -1.0f, -0.25f));
            sun.Light.Color = {1.0f, 0.97f, 0.90f};
            sun.Light.Intensity = 3.5f;
            sunActor->AddComponent<UDirectionalLightComponent>(sun);
        }
    }

    glm::vec3 AShooterGameMode::GetTeamSpawnLocation(EShooterTeam InTeam) const {
        auto occupied = [&](const glm::vec3& loc) {
            if (!World)
                return false;
            for (const auto& actor : World->GetAllActors()) {
                auto* ch = dynamic_cast<ACharacter*>(actor.get());
                if (!ch || ch->IsPendingKill())
                    continue;
                glm::vec3 d = ch->GetActorLocation() - loc;
                d.y = 0.0f;
                if (glm::length(d) < 1.4f)
                    return true;
            }
            return false;
        };

        const int32_t teamIndex = InTeam == EShooterTeam::Team2 ? 2 : 1;
        if (World) {
            std::vector<APlayerStart*> starts;
            for (const auto& actor : World->GetAllActors()) {
                auto* start = dynamic_cast<APlayerStart*>(actor.get());
                if (start && start->IsEnabled() && start->GetTeamIndex() == teamIndex)
                    starts.push_back(start);
            }
            if (!starts.empty()) {
                int32_t& cursor = InTeam == EShooterTeam::Team2 ? NextTeam2Spawn : NextTeam1Spawn;
                for (size_t n = 0; n < starts.size(); ++n) {
                    APlayerStart* pick = starts[static_cast<size_t>(cursor) % starts.size()];
                    ++cursor;
                    if (!occupied(pick->GetActorLocation()))
                        return pick->GetActorLocation();
                }
                APlayerStart* pick = starts[static_cast<size_t>(cursor) % starts.size()];
                return pick->GetActorLocation() + glm::vec3(1.6f, 0.0f, 0.8f);
            }
        }
        const auto& spawns = InTeam == EShooterTeam::Team2 ? Team2Spawns : Team1Spawns;
        if (spawns.empty())
            return {0.0f, 2.0f, 0.0f};
        int32_t& cursor = InTeam == EShooterTeam::Team2 ? NextTeam2Spawn : NextTeam1Spawn;
        glm::vec3 loc = spawns[static_cast<size_t>(cursor) % spawns.size()];
        ++cursor;
        if (occupied(loc))
            loc += glm::vec3(1.6f, 0.0f, 0.8f);
        return loc;
    }

    void AShooterGameMode::PossessHumanPawns() {
        if (!World)
            return;
        for (APlayerController* pc : World->GetPlayerControllers()) {
            if (!pc)
                continue;
            if (pc->GetPawn<AShooterCharacter>())
                continue;
            auto* ps = dynamic_cast<AShooterPlayerState*>(pc->GetPlayerState());
            if (ps && ps->GetTeam() == EShooterTeam::None)
                ps->SetTeam(AssignTeam());
            glm::vec3 spawn = GetTeamSpawnLocation(ps ? ps->GetTeam() : EShooterTeam::Team1);
            auto* pawn = World->SpawnActor<AShooterCharacter>("PlayerPawn");
            pawn->SetActorLocation(spawn);
            pawn->SetFloorZ(0.0f);
            pc->Possess(pawn);
            pc->SetInputModeGameOnly();
            pc->SetShowMouseCursor(false);
            ValidateSpawnedCharacter(*pawn, ps ? ps->GetTeam() : EShooterTeam::Team1);
        }
    }

    void AShooterGameMode::RequestStartMatch() {
        if (World && World->GetNetMode() == ENetMode::Client)
            return;
        StartMatch();
    }

    void AShooterGameMode::StartMatch() {
        // Flow: start TDM
        // 1. Build arena once; assign human teams; fill remaining slots with bots.
        // 2. Reset scores; possess every human PC; spawn a pawn per bot PC.
        // 3. Enter Starting countdown — combat is rejected until Playing.
        auto* gs = GetShooterGameState();
        if (gs &&
            (gs->GetMatchState() == EShooterMatchState::Starting || gs->GetMatchState() == EShooterMatchState::Playing))
            return;

        BuildArena();
        EnsurePlayableLighting();
        NextTeam1Spawn = 0;
        NextTeam2Spawn = 0;
        if (World) {
            if (auto* pc = World->GetFirstPlayerController()) {
                if (auto* ps = dynamic_cast<AShooterPlayerState*>(pc->GetPlayerState())) {
                    if (ps->GetTeam() == EShooterTeam::None)
                        ps->SetTeam(AssignTeam());
                    ps->SetIsBot(false);
                }
            }
        }
        FillBotsToCapacity();
        if (gs) {
            gs->SetMatchState(EShooterMatchState::Starting);
            gs->SetRemainingTime(Config.MatchDurationSeconds);
            gs->SetTeam1Kills(0);
            gs->SetTeam2Kills(0);
            gs->SetMatchWinner(EShooterMatchWinner::None);
        }
        if (GameState) {
            for (APlayerState* ps : GameState->GetPlayerArray()) {
                if (auto* sps = dynamic_cast<AShooterPlayerState*>(ps))
                    sps->ResetStats();
            }
        }
        StartingRemaining = Config.StartCountdownSeconds;
        if (gs)
            gs->SetCountdownRemaining(StartingRemaining);
        PossessHumanPawns();

        if (!World)
            return;
        for (AAIController* ai : World->GetAIControllers()) {
            auto* bot = dynamic_cast<AShooterBotController*>(ai);
            if (!bot)
                continue;
            if (bot->GetPawn<AShooterCharacter>())
                continue;
            auto* ps = dynamic_cast<AShooterPlayerState*>(bot->GetPlayerState());
            EShooterTeam team = ps ? ps->GetTeam() : EShooterTeam::Team2;
            glm::vec3 spawn = GetTeamSpawnLocation(team);
            auto* pawn = World->SpawnActor<AShooterCharacter>(ps ? ps->GetPlayerName() : "Bot");
            pawn->SetBotControlled(true);
            pawn->SetActorLocation(spawn);
            pawn->SetFloorZ(0.0f);
            bot->Possess(pawn);
            bot->SetWaypoints(Waypoints);
            bot->SetCoverPoints(CoverPoints);
            ValidateSpawnedCharacter(*pawn, team);
        }
        RefreshTeamCounts();
        DamageLog.clear();
        RespawnTimers.clear();
    }

    void AShooterGameMode::EndMatch(EShooterMatchWinner InWinner) {
        if (auto* gs = GetShooterGameState()) {
            gs->SetMatchState(EShooterMatchState::Finished);
            gs->SetMatchWinner(InWinner);
        }
        if (!World)
            return;
        for (APlayerController* pc : World->GetPlayerControllers()) {
            if (pc)
                pc->SetInputModeUIOnly();
        }
    }

    void AShooterGameMode::ReturnToMenu() {
        if (auto* gi = GetShooterGI())
            gi->ShutdownSession();
        EnterMainMenu();
        if (World)
            UGameplayStatics::OpenLevel(World, "/Game/Maps/MainMenu");
    }

    bool AShooterGameMode::CanDamage(const AShooterCharacter& InInstigator, const AShooterCharacter& InTarget) const {
        if (InTarget.GetHealthComponent() && InTarget.GetHealthComponent()->IsDead())
            return false;
        if (!Config.bFriendlyFire && InInstigator.GetTeam() != EShooterTeam::None &&
            InInstigator.GetTeam() == InTarget.GetTeam())
            return false;
        return true;
    }

    bool AShooterGameMode::ApplyAuthoritativeDamage(AShooterCharacter& InInstigator, AShooterCharacter& InTarget,
                                                    const FDamageInfo& InInfo) {
        if (!World || World->GetNetMode() == ENetMode::Client)
            return false;
        if (!IsCombatAllowed())
            return false;
        if (!CanDamage(InInstigator, InTarget))
            return false;

        InTarget.ApplyDamageFrom(InInfo);
        auto* gs = GetShooterGameState();
        auto* attackerPs = InInstigator.GetShooterPlayerState();
        DamageLog[&InTarget].push_back({attackerPs, gs ? gs->GetElapsedTime() : 0.0f, InInfo.DamageAmount});
        return true;
    }

    void AShooterGameMode::NotifyDeath(AShooterCharacter& InVictim, const FDamageInfo& InInfo) {
        auto* gs = GetShooterGameState();
        if (gs && gs->GetMatchState() == EShooterMatchState::Finished)
            return;
        auto* victimPs = InVictim.GetShooterPlayerState();
        if (victimPs)
            victimPs->AddDeath();

        AShooterPlayerState* killerPs = nullptr;
        if (auto* inst = dynamic_cast<AShooterCharacter*>(InInfo.Instigator))
            killerPs = inst->GetShooterPlayerState();

        if (killerPs && killerPs != victimPs) {
            killerPs->AddKill();
            if (gs)
                gs->AddTeamKill(killerPs->GetTeam());
        }

        const float now = gs ? gs->GetElapsedTime() : 0.0f;
        auto& credits = DamageLog[&InVictim];
        for (const auto& credit : credits) {
            if (!credit.Attacker || credit.Attacker == killerPs || credit.Attacker == victimPs)
                continue;
            if (now - credit.TimeSeconds <= Config.AssistWindowSeconds)
                credit.Attacker->AddAssist();
        }
        credits.clear();
        RespawnTimers[&InVictim] = Config.RespawnDelaySeconds;

        if (gs) {
            if (gs->GetTeam1Kills() >= Config.ScoreLimit)
                EndMatch(EShooterMatchWinner::Team1);
            else if (gs->GetTeam2Kills() >= Config.ScoreLimit)
                EndMatch(EShooterMatchWinner::Team2);
        }
    }

    void AShooterGameMode::RespawnCharacter(AShooterCharacter& InCharacter) {
        glm::vec3 loc = GetTeamSpawnLocation(InCharacter.GetTeam());
        InCharacter.OnServerRespawn(loc);
        if (auto* bot = dynamic_cast<AShooterBotController*>(InCharacter.GetController()))
            bot->NotifyRespawned();
    }

    void AShooterGameMode::TickMatch(float DeltaSeconds) {
        auto* gs = GetShooterGameState();
        if (!gs)
            return;
        if (gs->GetMatchState() == EShooterMatchState::Starting) {
            StartingRemaining -= DeltaSeconds;
            if (StartingRemaining <= 0.0f) {
                gs->SetMatchState(EShooterMatchState::Playing);
                gs->SetCountdownRemaining(0.0f);
            } else {
                gs->SetCountdownRemaining(StartingRemaining);
            }
            return;
        }
        if (gs->GetMatchState() != EShooterMatchState::Playing)
            return;

        gs->SetRemainingTime(std::max(0.0f, gs->GetRemainingTime() - DeltaSeconds));
        if (gs->GetRemainingTime() <= 0.0f) {
            if (gs->GetTeam1Kills() > gs->GetTeam2Kills())
                EndMatch(EShooterMatchWinner::Team1);
            else if (gs->GetTeam2Kills() > gs->GetTeam1Kills())
                EndMatch(EShooterMatchWinner::Team2);
            else
                EndMatch(EShooterMatchWinner::Draw);
        }

        std::vector<AShooterCharacter*> ready;
        for (auto& [ch, t] : RespawnTimers) {
            t -= DeltaSeconds;
            if (t <= 0.0f && ch)
                ready.push_back(ch);
        }
        for (auto* ch : ready) {
            RespawnTimers.erase(ch);
            RespawnCharacter(*ch);
        }
    }

    void AShooterGameMode::Tick(float DeltaSeconds) {
        AActor::Tick(DeltaSeconds);
        if (GetLocalRole() == ENetRole::SimulatedProxy)
            return;
        if (World && World->GetNetMode() == ENetMode::Client)
            return;
        TickMatch(DeltaSeconds);
    }

    std::vector<AShooterPlayerState*> AShooterGameMode::GetSortedScoreboard() const {
        if (auto* gs = GetShooterGameState())
            return gs->GetSortedScoreboard();
        return {};
    }

    void AShooterGameMode::ValidateSpawnedCharacter(AShooterCharacter& InCharacter, EShooterTeam InTeam) {
        if (!InCharacter.GetController())
            LE_CORE_ERROR("Spawn validation: '{0}' has no controller", InCharacter.GetName());
        if (InTeam == EShooterTeam::None)
            LE_CORE_ERROR("Spawn validation: '{0}' has no team", InCharacter.GetName());
        if (!InCharacter.GetWeapon())
            LE_CORE_ERROR("Spawn validation: '{0}' has no weapon", InCharacter.GetName());
        if (!InCharacter.GetHealthComponent() || InCharacter.GetHealthComponent()->IsDead())
            LE_CORE_ERROR("Spawn validation: '{0}' health is not ready", InCharacter.GetName());
        if (InCharacter.GetCapsuleHeight() < FWorldUnits::ExpectedHumanHeightMin ||
            InCharacter.GetCapsuleHeight() > FWorldUnits::ExpectedHumanHeightMax)
            LE_CORE_WARN("Spawn validation: '{0}' capsule height {1} m is outside human range", InCharacter.GetName(),
                         InCharacter.GetCapsuleHeight());
        if (World) {
            glm::vec3 minB, maxB;
            InCharacter.GetCapsuleAABB(minB, maxB);
            UWorld::FHitResult hit;
            if (World->OverlapAABB(minB, maxB, &InCharacter, hit) && hit.bBlockingHit)
                LE_CORE_WARN("Spawn validation: '{0}' overlaps '{1}'", InCharacter.GetName(),
                             hit.Actor ? hit.Actor->GetName() : "unknown");
        }
    }

} // namespace Leon
