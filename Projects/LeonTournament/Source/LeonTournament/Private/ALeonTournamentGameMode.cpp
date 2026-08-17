#include "ALeonTournamentGameMode.hpp"
#include "ALeonTournamentBotController.hpp"
#include "ALeonTournamentPlayerController.hpp"
#include "ULeonTournamentGameInstance.hpp"
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
#include "Core/FFrameProfiler.hpp"
#include "Core/FProjectPaths.hpp"
#include "Gameplay/UGameplayStatics.hpp"
#include "Renderer/FMeshPrimitives.hpp"
#include "Core/FWorldUnits.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace Leon {

    namespace {
        ULeonTournamentGameInstance* GetLeonTournamentGameInstance() {
            if (!UEngine::HasInstance())
                return nullptr;
            return dynamic_cast<ULeonTournamentGameInstance*>(UEngine::Get().GetGameInstance().get());
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

        bool WorldHasTeamPlayerStart(UWorld* InWorld, int32_t InTeamIndex) {
            if (!InWorld)
                return false;
            for (const auto& actor : InWorld->GetAllActors()) {
                auto* start = dynamic_cast<APlayerStart*>(actor.get());
                if (start && start->IsEnabled() && start->GetTeamIndex() == InTeamIndex)
                    return true;
            }
            return false;
        }

        void SetTravelGameModeClass(const std::string& InClassName) {
            if (!UEngine::HasInstance())
                return;
            FGameModeConfig cfg = UEngine::Get().GetGameModeConfig();
            cfg.GameModeClass = InClassName;
            UEngine::Get().SetGameModeConfig(cfg);
        }

        void FaceIntoArena(ALeonTournamentCharacter& InCharacter, ELeonTournamentTeam InTeam) {
            InCharacter.SetControlYaw(InTeam == ELeonTournamentTeam::Team2 ? 180.0f : 0.0f);
            InCharacter.SetControlPitch(0.0f);
            InCharacter.ApplyYawOnlyActorRotation();
        }
    } // namespace

    ALeonTournamentGameMode::ALeonTournamentGameMode(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AGameModeBase(InHandle, InWorld, InName) {
        SetClass("ALeonTournamentGameMode");
        DefaultPawnClass = "None";
        PlayerControllerClass = "ALeonTournamentPlayerController";
        HUDClass = "ALeonTournamentHUD";
        GameStateClass = "ALeonTournamentGameState";
        PlayerStateClass = "ALeonTournamentPlayerState";
    }

    ALeonTournamentGameState* ALeonTournamentGameMode::GetGameState() const {
        return dynamic_cast<ALeonTournamentGameState*>(GameState);
    }

    void ALeonTournamentGameMode::InitGame() {
        AGameModeBase::InitGame();
        if (auto* gs = GetGameState()) {
            gs->SetMatchState(ELeonTournamentMatchState::MainMenu);
            gs->SetRemainingTime(Config.MatchDurationSeconds);
        }
    }

    void ALeonTournamentGameMode::StartPlay() {
        if (World && World->GetNetMode() == ENetMode::Client)
            return;
        AGameModeBase::StartPlay();
        EnterMainMenu();
    }

    APlayerController* ALeonTournamentGameMode::Login(const std::string& InPlayerName) {
        APlayerController* pc = AGameModeBase::Login(InPlayerName);
        if (auto* ps = pc ? dynamic_cast<ALeonTournamentPlayerState*>(pc->GetPlayerState()) : nullptr) {
            if (ps->GetTeam() == ELeonTournamentTeam::None)
                ps->SetTeam(AssignTeam());
            ps->SetIsBot(false);
        }
        RefreshTeamCounts();
        return pc;
    }

    void ALeonTournamentGameMode::EnterMainMenu() {
        if (!IsNetworkAuthority())
            return;
        if (auto* gs = GetGameState()) {
            gs->SetMatchState(ELeonTournamentMatchState::MainMenu);
            gs->SetMatchWinner(ELeonTournamentMatchWinner::None);
            gs->SetTeam1Kills(0);
            gs->SetTeam2Kills(0);
        }
    }

    void ALeonTournamentGameMode::EnterLobby() {
        if (!IsNetworkAuthority())
            return;
        if (auto* gs = GetGameState())
            gs->SetMatchState(ELeonTournamentMatchState::Lobby);
        if (auto* pc = World ? World->GetFirstPlayerController() : nullptr) {
            if (auto* ps = dynamic_cast<ALeonTournamentPlayerState*>(pc->GetPlayerState())) {
                if (ps->GetTeam() == ELeonTournamentTeam::None)
                    ps->SetTeam(AssignTeam());
                ps->SetIsBot(false);
            }
        }
        if (ShouldFillBotsOnEnterLobby())
            FillBotsToCapacity();
        RefreshTeamCounts();
    }

    bool ALeonTournamentGameMode::ShouldFillBotsOnEnterLobby() const {
        if (World && World->GetNetMode() == ENetMode::Client)
            return false;
        if (auto* gi = GetLeonTournamentGameInstance())
            return gi->GetSessionMode() != ELeonTournamentSessionMode::LanHost;
        return true;
    }

    bool ALeonTournamentGameMode::IsCombatAllowed() const {
        auto* gs = GetGameState();
        return gs && gs->GetMatchState() == ELeonTournamentMatchState::Playing;
    }

    ELeonTournamentTeam ALeonTournamentGameMode::AssignTeam() {
        const int32_t t1 = CountTeam(ELeonTournamentTeam::Team1);
        const int32_t t2 = CountTeam(ELeonTournamentTeam::Team2);
        if (t1 <= t2 && t1 < Config.MaxTeamSize)
            return ELeonTournamentTeam::Team1;
        if (t2 < Config.MaxTeamSize)
            return ELeonTournamentTeam::Team2;
        return ELeonTournamentTeam::None;
    }

    int32_t ALeonTournamentGameMode::CountTeam(ELeonTournamentTeam InTeam) const {
        int32_t n = 0;
        if (!GameState)
            return 0;
        for (APlayerState* ps : GameState->GetPlayerArray()) {
            auto* sps = dynamic_cast<ALeonTournamentPlayerState*>(ps);
            if (sps && sps->GetTeam() == InTeam)
                ++n;
        }
        return n;
    }

    void ALeonTournamentGameMode::RefreshTeamCounts() {
        if (auto* gs = GetGameState()) {
            gs->SetTeam1PlayerCount(CountTeam(ELeonTournamentTeam::Team1));
            gs->SetTeam2PlayerCount(CountTeam(ELeonTournamentTeam::Team2));
        }
    }

    ALeonTournamentBotController* ALeonTournamentGameMode::SpawnBot(ELeonTournamentTeam InTeam,
                                                                    const std::string& InName) {
        if (!World)
            return nullptr;
        auto* bot = World->SpawnActor<ALeonTournamentBotController>(InName + "PC");
        auto* ps = World->SpawnActor<ALeonTournamentPlayerState>(InName + "PS");
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

    void ALeonTournamentGameMode::FillBotsToCapacity() {
        while (CountTeam(ELeonTournamentTeam::Team1) + CountTeam(ELeonTournamentTeam::Team2) < Config.MaxPlayers) {
            ELeonTournamentTeam team = AssignTeam();
            if (team == ELeonTournamentTeam::None)
                break;
            char name[32];
            std::snprintf(name, sizeof(name), "Bot_%d", NextBotId++);
            SpawnBot(team, name);
        }
        RefreshTeamCounts();
    }

    void ALeonTournamentGameMode::BuildArena() {
        if (bArenaBuilt || !World)
            return;
        bArenaBuilt = true;

        SpawnCollisionBox(World, "Floor", {0.0f, -0.25f, 0.0f}, {48.0f, 0.5f, 48.0f}, {0.22f, 0.24f, 0.26f});
        const glm::vec3 wallColor{0.16f, 0.18f, 0.20f};
        const glm::vec3 mazeColor{0.20f, 0.22f, 0.26f};
        const glm::vec3 coverColor{0.34f, 0.24f, 0.18f};
        SpawnCollisionBox(World, "WallN", {0.0f, 2.5f, -24.0f}, {48.0f, 5.0f, 0.7f}, wallColor);
        SpawnCollisionBox(World, "WallS", {0.0f, 2.5f, 24.0f}, {48.0f, 5.0f, 0.7f}, wallColor);
        SpawnCollisionBox(World, "WallW", {-24.0f, 2.5f, 0.0f}, {0.7f, 5.0f, 48.0f}, wallColor);
        SpawnCollisionBox(World, "WallE", {24.0f, 2.5f, 0.0f}, {0.7f, 5.0f, 48.0f}, wallColor);

        SpawnCollisionBox(World, "MazeW_A", {-8.0f, 2.25f, -20.0f}, {0.7f, 4.5f, 6.0f}, mazeColor);
        SpawnCollisionBox(World, "MazeW_B", {-8.0f, 2.25f, -8.0f}, {0.7f, 4.5f, 10.0f}, mazeColor);
        SpawnCollisionBox(World, "MazeW_C", {-8.0f, 2.25f, 8.0f}, {0.7f, 4.5f, 10.0f}, mazeColor);
        SpawnCollisionBox(World, "MazeW_D", {-8.0f, 2.25f, 20.0f}, {0.7f, 4.5f, 6.0f}, mazeColor);
        SpawnCollisionBox(World, "MazeE_A", {8.0f, 2.25f, -20.0f}, {0.7f, 4.5f, 6.0f}, mazeColor);
        SpawnCollisionBox(World, "MazeE_B", {8.0f, 2.25f, -8.0f}, {0.7f, 4.5f, 10.0f}, mazeColor);
        SpawnCollisionBox(World, "MazeE_C", {8.0f, 2.25f, 8.0f}, {0.7f, 4.5f, 10.0f}, mazeColor);
        SpawnCollisionBox(World, "MazeE_D", {8.0f, 2.25f, 20.0f}, {0.7f, 4.5f, 6.0f}, mazeColor);
        SpawnCollisionBox(World, "MazeN_A", {-20.0f, 2.25f, -8.0f}, {6.0f, 4.5f, 0.7f}, mazeColor);
        SpawnCollisionBox(World, "MazeN_B", {-8.0f, 2.25f, -8.0f}, {10.0f, 4.5f, 0.7f}, mazeColor);
        SpawnCollisionBox(World, "MazeN_C", {8.0f, 2.25f, -8.0f}, {10.0f, 4.5f, 0.7f}, mazeColor);
        SpawnCollisionBox(World, "MazeN_D", {20.0f, 2.25f, -8.0f}, {6.0f, 4.5f, 0.7f}, mazeColor);
        SpawnCollisionBox(World, "MazeS_A", {-20.0f, 2.25f, 8.0f}, {6.0f, 4.5f, 0.7f}, mazeColor);
        SpawnCollisionBox(World, "MazeS_B", {-8.0f, 2.25f, 8.0f}, {10.0f, 4.5f, 0.7f}, mazeColor);
        SpawnCollisionBox(World, "MazeS_C", {8.0f, 2.25f, 8.0f}, {10.0f, 4.5f, 0.7f}, mazeColor);
        SpawnCollisionBox(World, "MazeS_D", {20.0f, 2.25f, 8.0f}, {6.0f, 4.5f, 0.7f}, mazeColor);

        SpawnCollisionBox(World, "CoverA", {-14.0f, 1.15f, -14.0f}, {3.2f, 2.3f, 1.2f}, coverColor);
        SpawnCollisionBox(World, "CoverB", {14.0f, 1.15f, 14.0f}, {3.2f, 2.3f, 1.2f}, {0.18f, 0.28f, 0.40f});
        SpawnCollisionBox(World, "CoverC", {-14.0f, 1.15f, 14.0f}, {1.4f, 2.3f, 3.2f}, {0.30f, 0.30f, 0.20f});
        SpawnCollisionBox(World, "CoverD", {14.0f, 1.15f, -14.0f}, {1.4f, 2.3f, 3.2f}, {0.30f, 0.30f, 0.20f});
        SpawnCollisionBox(World, "CoverMidW", {-3.0f, 1.15f, 0.0f}, {2.4f, 2.3f, 1.1f}, coverColor);
        SpawnCollisionBox(World, "CoverMidE", {3.0f, 1.15f, 0.0f}, {2.4f, 2.3f, 1.1f}, {0.20f, 0.26f, 0.34f});
        SpawnCollisionBox(World, "RampL", {-16.0f, 0.6f, 0.0f}, {4.0f, 1.2f, 2.0f}, {0.28f, 0.28f, 0.30f});
        SpawnCollisionBox(World, "RampR", {16.0f, 0.6f, 0.0f}, {4.0f, 1.2f, 2.0f}, {0.28f, 0.28f, 0.30f});

        SpawnCollisionBox(World, "ScaleCube1m", {0.0f, 0.5f, -20.0f}, {1.0f, 1.0f, 1.0f}, {0.9f, 0.2f, 0.2f});
        SpawnCollisionBox(World, "ScaleCube10cm", {1.2f, 0.05f, -20.0f}, {0.1f, 0.1f, 0.1f}, {0.2f, 0.9f, 0.2f});

        Team1Spawns = {{-18.0f, 2.0f, -16.0f}, {-18.0f, 2.0f, -4.0f}, {-18.0f, 2.0f, 4.0f}, {-18.0f, 2.0f, 16.0f},
                       {-16.0f, 2.0f, -16.0f}, {-16.0f, 2.0f, 16.0f}, {-20.0f, 2.0f, 0.0f}, {-14.0f, 2.0f, 0.0f}};
        Team2Spawns = {{18.0f, 2.0f, 16.0f}, {18.0f, 2.0f, 4.0f},   {18.0f, 2.0f, -4.0f}, {18.0f, 2.0f, -16.0f},
                       {16.0f, 2.0f, 16.0f}, {16.0f, 2.0f, -16.0f}, {20.0f, 2.0f, 0.0f},  {14.0f, 2.0f, 0.0f}};
        Waypoints = {{-16.0f, 2.0f, -16.0f}, {-16.0f, 2.0f, 0.0f},  {-16.0f, 2.0f, 16.0f}, {0.0f, 2.0f, -16.0f},
                     {0.0f, 2.0f, 0.0f},     {0.0f, 2.0f, 16.0f},   {16.0f, 2.0f, -16.0f}, {16.0f, 2.0f, 0.0f},
                     {16.0f, 2.0f, 16.0f},   {-8.0f, 2.0f, 0.0f},   {8.0f, 2.0f, 0.0f},    {0.0f, 2.0f, -8.0f},
                     {0.0f, 2.0f, 8.0f},     {-16.0f, 2.0f, -4.0f}, {-16.0f, 2.0f, 4.0f},  {16.0f, 2.0f, -4.0f},
                     {16.0f, 2.0f, 4.0f},    {-4.0f, 2.0f, -16.0f}, {4.0f, 2.0f, -16.0f},  {-4.0f, 2.0f, 16.0f},
                     {4.0f, 2.0f, 16.0f}};
        CoverPoints = {{-16.0f, 2.0f, -12.0f}, {-12.0f, 2.0f, -16.0f}, {16.0f, 2.0f, 12.0f},  {12.0f, 2.0f, 16.0f},
                       {-16.0f, 2.0f, 12.0f},  {-12.0f, 2.0f, 16.0f},  {16.0f, 2.0f, -12.0f}, {12.0f, 2.0f, -16.0f},
                       {-5.0f, 2.0f, 0.0f},    {5.0f, 2.0f, 0.0f},     {0.0f, 2.0f, -10.0f},  {0.0f, 2.0f, 10.0f}};

        if (!WorldHasTeamPlayerStart(World, 1)) {
            for (size_t i = 0; i < Team1Spawns.size(); ++i) {
                auto* start = World->SpawnActor<APlayerStart>("Team1Start_" + std::to_string(i));
                start->SetActorLocation(Team1Spawns[i]);
                start->SetTeamIndex(1);
                start->SetPlayerStartTag("Team1");
            }
        }
        if (!WorldHasTeamPlayerStart(World, 2)) {
            for (size_t i = 0; i < Team2Spawns.size(); ++i) {
                auto* start = World->SpawnActor<APlayerStart>("Team2Start_" + std::to_string(i));
                start->SetActorLocation(Team2Spawns[i]);
                start->SetTeamIndex(2);
                start->SetPlayerStartTag("Team2");
            }
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

    void ALeonTournamentGameMode::EnsurePlayableLighting() {
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

    glm::vec3 ALeonTournamentGameMode::GetTeamSpawnLocation(ELeonTournamentTeam InTeam) const {
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

        const int32_t teamIndex = InTeam == ELeonTournamentTeam::Team2 ? 2 : 1;
        if (World) {
            std::vector<APlayerStart*> starts;
            for (const auto& actor : World->GetAllActors()) {
                auto* start = dynamic_cast<APlayerStart*>(actor.get());
                if (start && start->IsEnabled() && start->GetTeamIndex() == teamIndex)
                    starts.push_back(start);
            }
            if (!starts.empty()) {
                int32_t& cursor = InTeam == ELeonTournamentTeam::Team2 ? NextTeam2Spawn : NextTeam1Spawn;
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
        const auto& spawns = InTeam == ELeonTournamentTeam::Team2 ? Team2Spawns : Team1Spawns;
        if (spawns.empty())
            return {0.0f, 2.0f, 0.0f};
        int32_t& cursor = InTeam == ELeonTournamentTeam::Team2 ? NextTeam2Spawn : NextTeam1Spawn;
        glm::vec3 loc = spawns[static_cast<size_t>(cursor) % spawns.size()];
        ++cursor;
        if (occupied(loc))
            loc += glm::vec3(1.6f, 0.0f, 0.8f);
        return loc;
    }

    void ALeonTournamentGameMode::PossessHumanPawns() {
        if (!World)
            return;
        for (APlayerController* pc : World->GetPlayerControllers()) {
            if (pc)
                RestartPlayer(pc);
        }
    }

    void ALeonTournamentGameMode::RestartPlayer(AController* NewPlayer) {
        if (!NewPlayer || !World || !IsNetworkAuthority())
            return;

        auto* ps = dynamic_cast<ALeonTournamentPlayerState*>(NewPlayer->GetPlayerState());
        if (ps && ps->GetTeam() == ELeonTournamentTeam::None)
            ps->SetTeam(AssignTeam());
        const ELeonTournamentTeam team = ps ? ps->GetTeam() : ELeonTournamentTeam::Team1;
        const glm::vec3 spawn = GetTeamSpawnLocation(team);
        const bool bBot = dynamic_cast<ALeonTournamentBotController*>(NewPlayer) != nullptr;

        if (APawn* oldPawn = NewPlayer->GetPawn()) {
            if (auto* oldChar = dynamic_cast<ALeonTournamentCharacter*>(oldPawn))
                DamageLog.erase(oldChar);
            NewPlayer->UnPossess();
            World->DestroyActor(oldPawn);
        }

        const std::string pawnName = (bBot && ps) ? ps->GetPlayerName() : "PlayerPawn";
        auto* pawn = World->SpawnActor<ALeonTournamentCharacter>(pawnName);
        pawn->SetBotControlled(bBot);
        pawn->SetActorLocation(spawn);
        pawn->SetFloorZ(0.0f);
        NewPlayer->Possess(pawn);
        FaceIntoArena(*pawn, team);
        if (auto* bot = dynamic_cast<ALeonTournamentBotController*>(NewPlayer)) {
            bot->SetWaypoints(Waypoints);
            bot->SetCoverPoints(CoverPoints);
            bot->NotifyRespawned();
        }
        ValidateSpawnedCharacter(*pawn, team);
    }

    void ALeonTournamentGameMode::RequestStartMatch() {
        if (World && World->GetNetMode() == ENetMode::Client)
            return;
        StartMatch();
    }

    void ALeonTournamentGameMode::OpenAnimLab() {
        if (!IsNetworkAuthority() || !UEngine::HasInstance())
            return;
        SetTravelGameModeClass("ALeonTournamentAnimLabGameMode");
        if (World)
            UGameplayStatics::OpenLevel(World, "/Game/Maps/AnimLab");
    }

    void ALeonTournamentGameMode::StartMatch() {
        // Flow: start TDM
        // 1. Build arena once; assign human teams; fill remaining slots with bots.
        // 2. Reset scores; RestartPlayer every human PC and bot (new pawn, same PlayerState).
        // 3. Enter Starting countdown — combat is rejected until Playing.
        if (!IsNetworkAuthority())
            return;
        auto* gs = GetGameState();
        if (gs && (gs->GetMatchState() == ELeonTournamentMatchState::Starting ||
                   gs->GetMatchState() == ELeonTournamentMatchState::Playing))
            return;

        BuildArena();
        EnsurePlayableLighting();
        NextTeam1Spawn = 0;
        NextTeam2Spawn = 0;
        if (World) {
            if (auto* pc = World->GetFirstPlayerController()) {
                if (auto* ps = dynamic_cast<ALeonTournamentPlayerState*>(pc->GetPlayerState())) {
                    if (ps->GetTeam() == ELeonTournamentTeam::None)
                        ps->SetTeam(AssignTeam());
                    ps->SetIsBot(false);
                }
            }
        }
        FillBotsToCapacity();
        if (gs) {
            gs->SetMatchState(ELeonTournamentMatchState::Starting);
            gs->SetRemainingTime(Config.MatchDurationSeconds);
            gs->SetTeam1Kills(0);
            gs->SetTeam2Kills(0);
            gs->SetMatchWinner(ELeonTournamentMatchWinner::None);
        }
        if (GameState) {
            for (APlayerState* ps : GameState->GetPlayerArray()) {
                if (auto* sps = dynamic_cast<ALeonTournamentPlayerState*>(ps))
                    sps->ResetStats();
            }
        }
        StartingRemaining = Config.StartCountdownSeconds;
        if (gs)
            gs->SetCountdownRemaining(StartingRemaining);
        PossessHumanPawns();

        if (World) {
            for (AAIController* ai : World->GetAIControllers()) {
                if (auto* bot = dynamic_cast<ALeonTournamentBotController*>(ai))
                    RestartPlayer(bot);
            }
        }
        RefreshTeamCounts();
        DamageLog.clear();
        RespawnTimers.clear();
    }

    void ALeonTournamentGameMode::EndMatch(ELeonTournamentMatchWinner InWinner) {
        if (!IsNetworkAuthority())
            return;
        if (auto* gs = GetGameState()) {
            gs->SetMatchState(ELeonTournamentMatchState::Finished);
            gs->SetMatchWinner(InWinner);
        }
    }

    void ALeonTournamentGameMode::ReturnToMenu() {
        if (!IsNetworkAuthority())
            return;
        if (auto* gi = GetLeonTournamentGameInstance())
            gi->ShutdownSession();
        EnterMainMenu();
        SetTravelGameModeClass("ALeonTournamentGameMode");
        if (World)
            UGameplayStatics::OpenLevel(World, "/Game/Maps/MainMenu");
    }

    bool ALeonTournamentGameMode::CanDamage(const ALeonTournamentCharacter& InInstigator,
                                            const ALeonTournamentCharacter& InTarget) const {
        if (InTarget.GetHealthComponent() && InTarget.GetHealthComponent()->IsDead())
            return false;
        if (!Config.bFriendlyFire && InInstigator.GetTeam() != ELeonTournamentTeam::None &&
            InInstigator.GetTeam() == InTarget.GetTeam())
            return false;
        return true;
    }

    bool ALeonTournamentGameMode::ApplyAuthoritativeDamage(ALeonTournamentCharacter& InInstigator,
                                                           ALeonTournamentCharacter& InTarget,
                                                           const FDamageInfo& InInfo) {
        if (!World || World->GetNetMode() == ENetMode::Client)
            return false;
        if (!IsCombatAllowed())
            return false;
        if (!CanDamage(InInstigator, InTarget))
            return false;

        InTarget.ApplyDamageFrom(InInfo);
        auto* gs = GetGameState();
        auto* attackerPs = InInstigator.GetPlayerState();
        DamageLog[&InTarget].push_back({attackerPs, gs ? gs->GetElapsedTime() : 0.0f, InInfo.DamageAmount});
        return true;
    }

    void ALeonTournamentGameMode::NotifyDeath(ALeonTournamentCharacter& InVictim, const FDamageInfo& InInfo) {
        if (!IsNetworkAuthority())
            return;
        auto* gs = GetGameState();
        if (gs && gs->GetMatchState() == ELeonTournamentMatchState::Finished)
            return;
        auto* victimPs = InVictim.GetPlayerState();
        if (victimPs)
            victimPs->AddDeath();

        ALeonTournamentPlayerState* killerPs = nullptr;
        if (auto* inst = dynamic_cast<ALeonTournamentCharacter*>(InInfo.Instigator))
            killerPs = inst->GetPlayerState();

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
        if (AController* ctrl = InVictim.GetController())
            RespawnTimers[ctrl] = Config.RespawnDelaySeconds;

        if (gs) {
            if (gs->GetTeam1Kills() >= Config.ScoreLimit)
                EndMatch(ELeonTournamentMatchWinner::Team1);
            else if (gs->GetTeam2Kills() >= Config.ScoreLimit)
                EndMatch(ELeonTournamentMatchWinner::Team2);
        }
    }

    void ALeonTournamentGameMode::RespawnCharacter(ALeonTournamentCharacter& InCharacter) {
        AController* ctrl = InCharacter.GetController();
        if (!ctrl)
            return;
        RespawnTimers.erase(ctrl);
        RestartPlayer(ctrl);
    }

    void ALeonTournamentGameMode::TickMatch(float DeltaSeconds) {
        auto* gs = GetGameState();
        if (!gs)
            return;
        if (gs->GetMatchState() == ELeonTournamentMatchState::Starting) {
            StartingRemaining -= DeltaSeconds;
            if (StartingRemaining <= 0.0f) {
                gs->SetMatchState(ELeonTournamentMatchState::Playing);
                gs->SetCountdownRemaining(0.0f);
            } else {
                gs->SetCountdownRemaining(StartingRemaining);
            }
            return;
        }
        if (gs->GetMatchState() != ELeonTournamentMatchState::Playing)
            return;

        gs->SetRemainingTime(std::max(0.0f, gs->GetRemainingTime() - DeltaSeconds));
        if (gs->GetRemainingTime() <= 0.0f) {
            if (gs->GetTeam1Kills() > gs->GetTeam2Kills())
                EndMatch(ELeonTournamentMatchWinner::Team1);
            else if (gs->GetTeam2Kills() > gs->GetTeam1Kills())
                EndMatch(ELeonTournamentMatchWinner::Team2);
            else
                EndMatch(ELeonTournamentMatchWinner::Draw);
        }

        std::vector<AController*> ready;
        for (auto& [ctrl, t] : RespawnTimers) {
            t -= DeltaSeconds;
            if (t <= 0.0f && ctrl && !ctrl->IsPendingKill())
                ready.push_back(ctrl);
        }
        for (auto* ctrl : ready) {
            RespawnTimers.erase(ctrl);
            RestartPlayer(ctrl);
        }
    }

    void ALeonTournamentGameMode::TickAutoPlay(float DeltaSeconds) {
        auto* gi = GetLeonTournamentGameInstance();
        if (!gi || !gi->IsAutoOfflineMatch() || bAutoPlayFinished)
            return;
        auto* gs = GetGameState();
        if (!gs)
            return;

        if (FApplication::HasInstance() && FApplication::Get().GetWindow().IsVSync())
            FApplication::Get().GetWindow().SetVSync(false);

        switch (gs->GetMatchState()) {
        case ELeonTournamentMatchState::MainMenu:
            gi->SetSessionMode(ELeonTournamentSessionMode::Offline);
            EnterLobby();
            return;
        case ELeonTournamentMatchState::Lobby:
            StartMatch();
            return;
        case ELeonTournamentMatchState::Starting:
            return;
        case ELeonTournamentMatchState::Finished:
            WriteAutoPlayReport();
            return;
        default:
            break;
        }

        AutoPlayElapsed += DeltaSeconds;
        const auto& timing = FFrameProfiler::Last();
        if (timing.FrameMs > 0.05f && AutoPlayElapsed > 1.0f) {
            AutoPlayMsSum += timing.FrameMs;
            AutoPlayMsMin = std::min(AutoPlayMsMin, timing.FrameMs);
            AutoPlayMsMax = std::max(AutoPlayMsMax, timing.FrameMs);
            ++AutoPlaySamples;
        }
        if (AutoPlayElapsed >= gi->GetAutoMatchSeconds())
            WriteAutoPlayReport();
        else if (static_cast<int>(AutoPlayElapsed) / 5 != static_cast<int>(AutoPlayElapsed - DeltaSeconds) / 5) {
            const auto& t = FFrameProfiler::Last();
            LE_CORE_INFO(
                "AutoPlay t={:.0f}s frame={:.2f}ms game={:.2f} render={:.2f} shadow={:.2f} anim={:.2f} ai={:.2f}",
                AutoPlayElapsed, t.FrameMs, t.GameMs, t.RenderMs, t.ShadowMs, t.AnimationMs, t.AIMs);
        }
    }

    void ALeonTournamentGameMode::WriteAutoPlayReport() {
        if (bAutoPlayFinished)
            return;
        bAutoPlayFinished = true;
        auto* gi = GetLeonTournamentGameInstance();
        auto* gs = GetGameState();
        std::string path = gi ? gi->GetAutoReportPath() : "";
        std::filesystem::path fallback =
            std::filesystem::path(FProjectPaths::ProjectSavedDir()) / "offline_match_report.txt";
        std::error_code ec;
        std::filesystem::create_directories(fallback.parent_path(), ec);
        if (path.empty())
            path = fallback.string();
        else {
            std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
        }

        const auto& timing = FFrameProfiler::Last();
        const float avgMs = AutoPlaySamples > 0 ? AutoPlayMsSum / static_cast<float>(AutoPlaySamples) : timing.FrameMs;
        const float avgFps = avgMs > 0.01f ? 1000.0f / avgMs : 0.0f;

        int32_t bots = 0, moving = 0, combat = 0, reloading = 0, dead = 0, searching = 0;
        int32_t characters = 0, thirdPerson = 0, weapons = 0, falling = 0;
        if (World) {
            for (AAIController* ai : World->GetAIControllers()) {
                auto* bot = dynamic_cast<ALeonTournamentBotController*>(ai);
                if (!bot)
                    continue;
                ++bots;
                switch (bot->GetBotState()) {
                case ELeonTournamentBotState::Search:
                    ++searching;
                    break;
                case ELeonTournamentBotState::Combat:
                case ELeonTournamentBotState::Aim:
                case ELeonTournamentBotState::Fire:
                case ELeonTournamentBotState::MoveToTarget:
                    ++combat;
                    break;
                case ELeonTournamentBotState::Reload:
                    ++reloading;
                    break;
                case ELeonTournamentBotState::Dead:
                    ++dead;
                    break;
                default:
                    if (bot->GetMoveStatus() == EPathFollowingStatus::Moving)
                        ++moving;
                    break;
                }
                if (bot->GetMoveStatus() == EPathFollowingStatus::Moving)
                    ++moving;
            }
            for (const auto& actor : World->GetAllActors()) {
                auto* ch = dynamic_cast<ALeonTournamentCharacter*>(actor.get());
                if (!ch)
                    continue;
                ++characters;
                if (ch->IsThirdPerson())
                    ++thirdPerson;
                if (ch->GetWeapon())
                    ++weapons;
                if (ch->IsFalling())
                    ++falling;
            }
        }

        std::ofstream out(path);
        if (!out) {
            path = fallback.string();
            out.open(path);
        }
        if (out) {
            out << "LeonTournament offline match report\n";
            out << "elapsed_s=" << AutoPlayElapsed << "\n";
            out << "match_state=" << (gs ? static_cast<int>(gs->GetMatchState()) : -1) << "\n";
            out << "team1_kills=" << (gs ? gs->GetTeam1Kills() : 0) << "\n";
            out << "team2_kills=" << (gs ? gs->GetTeam2Kills() : 0) << "\n";
            out << "team1_players=" << (gs ? gs->GetTeam1PlayerCount() : 0) << "\n";
            out << "team2_players=" << (gs ? gs->GetTeam2PlayerCount() : 0) << "\n";
            out << "characters=" << characters << "\n";
            out << "third_person=" << thirdPerson << "\n";
            out << "weapons=" << weapons << "\n";
            out << "falling=" << falling << "\n";
            out << "bots=" << bots << " searching=" << searching << " combat=" << combat << " moving=" << moving
                << " reloading=" << reloading << " dead=" << dead << "\n";
            out << "avg_frame_ms=" << avgMs << "\n";
            out << "avg_fps=" << avgFps << "\n";
            out << "min_frame_ms=" << (AutoPlaySamples > 0 ? AutoPlayMsMin : timing.FrameMs) << "\n";
            out << "max_frame_ms=" << AutoPlayMsMax << "\n";
            out << "samples=" << AutoPlaySamples << "\n";
            out << "last_game_ms=" << timing.GameMs << "\n";
            out << "last_render_ms=" << timing.RenderMs << "\n";
            out << "last_shadow_ms=" << timing.ShadowMs << "\n";
            out << "last_anim_ms=" << timing.AnimationMs << "\n";
            out << "last_ai_ms=" << timing.AIMs << "\n";
            out << "last_physics_ms=" << timing.PhysicsMs << "\n";
            out << "last_ui_ms=" << timing.UIMs << "\n";
            out << "last_gpu_ms=" << timing.GPUMs << "\n";
            out << "shadow_draw_calls=" << timing.ShadowDrawCalls << "\n";
            out << "particles=" << timing.ParticleCount << "\n";
            out << "visible_actors=" << timing.VisibleActors << "\n";
        }
        LE_CORE_INFO("AutoPlay report written to {}  avg={:.1f} ms ({:.0f} FPS)", path, avgMs, avgFps);
        if (FApplication::HasInstance())
            FApplication::Get().Close();
    }

    void ALeonTournamentGameMode::EndPlay() {
        DamageLog.clear();
        RespawnTimers.clear();
        if (!bAutoPlayFinished) {
            auto* gi = GetLeonTournamentGameInstance();
            if (gi && gi->IsAutoOfflineMatch())
                WriteAutoPlayReport();
        }
        AGameModeBase::EndPlay();
    }

    void ALeonTournamentGameMode::Tick(float DeltaSeconds) {
        AActor::Tick(DeltaSeconds);
        if (GetLocalRole() == ENetRole::SimulatedProxy)
            return;
        if (World && World->GetNetMode() == ENetMode::Client)
            return;
        TickAutoPlay(DeltaSeconds);
        TickMatch(DeltaSeconds);
    }

    std::vector<ALeonTournamentPlayerState*> ALeonTournamentGameMode::GetSortedScoreboard() const {
        if (auto* gs = GetGameState())
            return gs->GetSortedScoreboard();
        return {};
    }

    void ALeonTournamentGameMode::ValidateSpawnedCharacter(ALeonTournamentCharacter& InCharacter,
                                                           ELeonTournamentTeam InTeam) {
        if (!InCharacter.GetController())
            LE_CORE_ERROR("Spawn validation: '{0}' has no controller", InCharacter.GetName());
        if (InTeam == ELeonTournamentTeam::None)
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
