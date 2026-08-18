#include "ALeonTournamentGameMode.hpp"
#include "ALeonTournamentBotController.hpp"
#include "ALeonTournamentPlayerController.hpp"
#include "ALeonTournamentPickup.hpp"
#include "FLeonTournamentArenaBuilder.hpp"
#include "FLeonTournamentDamageRules.hpp"
#include "ULeonTournamentGameInstance.hpp"
#include "Assets/UAssetManager.hpp"
#include "Core/FApplication.hpp"
#include "Gameplay/AActor.hpp"
#include "Gameplay/UPrimitiveComponent.hpp"
#include "Gameplay/AAIController.hpp"
#include "Gameplay/APlayerStart.hpp"
#include "Gameplay/ANavMeshBoundsVolume.hpp"
#include "Gameplay/ACharacter.hpp"
#include "AI/UNavigationSystem.hpp"
#include "Renderer/FWorldRenderer.hpp"
#include "Engine/Components.hpp"
#include "Assets/UStaticMesh.hpp"
#include "Engine/UEngine.hpp"
#include "Engine/UWorld.hpp"
#include "Engine/FMapSerializer.hpp"
#include "Core/FLog.hpp"
#include "Core/FFrameProfiler.hpp"
#include "Core/FProjectPaths.hpp"
#include "Gameplay/UGameplayStatics.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Gameplay/APlayerCameraManager.hpp"
#include "Lightmass/FLightmass.hpp"
#include "Renderer/FMeshPrimitives.hpp"
#include "Renderer/FMaterial.hpp"
#include "Renderer/FPerspectiveCamera.hpp"
#include "UMG/FUILayout.hpp"
#include "Core/FWorldUnits.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

namespace Leon {

    namespace {
        ULeonTournamentGameInstance* GetLeonTournamentGameInstance() {
            if (!UEngine::HasInstance())
                return nullptr;
            return dynamic_cast<ULeonTournamentGameInstance*>(UEngine::Get().GetGameInstance().get());
        }

        ELeonTournamentCharacterSkin RandomBotCharacterSkin() {
            static thread_local std::mt19937 rng{std::random_device{}()};
            std::uniform_int_distribution<int> dist(0, static_cast<int>(ELeonTournamentCharacterSkin::Count) - 1);
            return static_cast<ELeonTournamentCharacterSkin>(dist(rng));
        }

        void SyncLocalPlayerCharacterSkin(ALeonTournamentPlayerState* InPs) {
            if (!InPs || InPs->IsBot())
                return;
            if (auto* gi = GetLeonTournamentGameInstance())
                InPs->SetCharacterSkin(gi->GetSelectedCharacterSkin());
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

        bool IsNightArenaMap() {
            if (!UEngine::HasInstance())
                return false;
            return UEngine::Get().GetCurrentMapName().find("Night") != std::string::npos;
        }

        bool IsOrbitalPrismMap() {
            if (!UEngine::HasInstance())
                return false;
            return UEngine::Get().GetCurrentMapName().find("OrbitalPrism") != std::string::npos;
        }

        bool IsAuthoredPlayableMap() {
            // Night uses procedural cubes in the .lmap (not FStaticMeshComponent), but lighting
            // and geometry are authored — do not rebuild them at match start.
            return IsNightArenaMap() || IsOrbitalPrismMap();
        }

        bool WorldHasImportedStaticMeshes(UWorld* InWorld) {
            if (!InWorld)
                return false;
            for (const auto& actor : InWorld->GetAllActors()) {
                if (actor && actor->HasComponent<FStaticMeshComponent>())
                    return true;
            }
            return false;
        }

        const char* ArenaLightmapPath() {
            return IsNightArenaMap() ? "/Game/Lightmaps/TournamentArenaNight.llightmap"
                                     : "/Game/Lightmaps/TournamentArena.llightmap";
        }

        const char* ArenaMapFileName() {
            return IsNightArenaMap() ? "TournamentArenaNight.lmap" : "TournamentArena.lmap";
        }

        void SpawnNightArenaLights(UWorld* InWorld) {
            if (!InWorld)
                return;
            const glm::vec3 warm{1.0f, 0.72f, 0.38f};
            const glm::vec3 cool{0.45f, 0.68f, 1.0f};
            const glm::vec3 sodium{1.0f, 0.62f, 0.22f};
            // Stationary: runtime direct in the UBO + Lightmass bounce. Static would be bake-only
            // and the night scene went black if the atlas was dim.
            FLeonTournamentArenaBuilder::SpawnPointLight(InWorld, "Night_PL_N", {0.0f, 7.5f, -22.0f}, warm, 12.0f,
                                                         18.0f, ELightMobility::Stationary);
            FLeonTournamentArenaBuilder::SpawnPointLight(InWorld, "Night_PL_S", {0.0f, 7.5f, 22.0f}, warm, 12.0f, 18.0f,
                                                         ELightMobility::Stationary);
            FLeonTournamentArenaBuilder::SpawnPointLight(InWorld, "Night_PL_W", {-22.0f, 7.5f, 0.0f}, sodium, 11.0f,
                                                         17.0f, ELightMobility::Stationary);
            FLeonTournamentArenaBuilder::SpawnPointLight(InWorld, "Night_PL_E", {22.0f, 7.5f, 0.0f}, sodium, 11.0f,
                                                         17.0f, ELightMobility::Stationary);
            FLeonTournamentArenaBuilder::SpawnPointLight(InWorld, "Night_PL_NW", {-18.0f, 6.5f, -18.0f}, warm, 10.0f,
                                                         16.0f, ELightMobility::Stationary);
            FLeonTournamentArenaBuilder::SpawnPointLight(InWorld, "Night_PL_SE", {18.0f, 6.5f, 18.0f}, warm, 10.0f,
                                                         16.0f, ELightMobility::Stationary);
            FLeonTournamentArenaBuilder::SpawnPointLight(InWorld, "Night_PL_Mid", {0.0f, 8.0f, 0.0f}, cool, 9.0f, 20.0f,
                                                         ELightMobility::Stationary);
            FLeonTournamentArenaBuilder::SpawnSpotLight(InWorld, "Night_Spot_Mid", {0.0f, 12.0f, 0.0f},
                                                        {0.0f, -1.0f, 0.0f}, cool, 22.0f, 28.0f, 16.0f, 28.0f,
                                                        ELightMobility::Stationary);
            FLeonTournamentArenaBuilder::SpawnSpotLight(InWorld, "Night_Spot_T1", {-20.0f, 11.0f, -20.0f},
                                                        {0.25f, -1.0f, 0.25f}, warm, 18.0f, 22.0f, 14.0f, 26.0f,
                                                        ELightMobility::Stationary);
            FLeonTournamentArenaBuilder::SpawnSpotLight(InWorld, "Night_Spot_T2", {20.0f, 11.0f, 20.0f},
                                                        {-0.25f, -1.0f, -0.25f}, warm, 18.0f, 22.0f, 14.0f, 26.0f,
                                                        ELightMobility::Stationary);
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

        // Flow: match start after world BeginPlay
        // 1. UWorld::BeginPlay defers actor BeginPlay while StartPlay runs (Login/HUD wiring).
        // 2. StartMatch spawns pawns that need BeginPlay/EnsureWeapon before spawn validation.
        // 3. If still deferring, schedule StartMatch for the next tick (timer rate 0).
        auto* gi = GetLeonTournamentGameInstance();
        const bool bShouldStartMatch =
            (gi && gi->ConsumePendingMatchStart()) || IsNightArenaMap() || IsOrbitalPrismMap();
        if (!bShouldStartMatch) {
            EnterMainMenu();
            return;
        }
        if (World && World->IsDeferringSpawnedActorBeginPlay()) {
            World->GetTimerManager().SetTimer(
                PendingStartMatchHandle,
                [this]() {
                    if (!IsPendingKill())
                        StartMatch();
                },
                0.0f, false);
        } else {
            StartMatch();
        }
    }

    APlayerController* ALeonTournamentGameMode::Login(const std::string& InPlayerName) {
        APlayerController* pc = AGameModeBase::Login(InPlayerName);
        if (auto* ps = pc ? dynamic_cast<ALeonTournamentPlayerState*>(pc->GetPlayerState()) : nullptr) {
            if (ps->GetTeam() == ELeonTournamentTeam::None)
                ps->SetTeam(AssignTeam());
            ps->SetIsBot(false);
            SyncLocalPlayerCharacterSkin(ps);
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
        EnsureMenuShowcase();
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
                SyncLocalPlayerCharacterSkin(ps);
            }
        }
        EnsureMenuShowcase();
        if (ShouldFillBotsOnEnterLobby())
            SyncLobbyBots();
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

    void ALeonTournamentGameMode::ApplyMatchCapacityFromLobby() {
        if (!GetLeonTournamentGameInstance())
            return;
        Config.MaxPlayers = 12;
        Config.MaxTeamSize = 6;
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

    int32_t ALeonTournamentGameMode::CountHumans() const {
        int32_t n = 0;
        if (!GameState)
            return 0;
        for (APlayerState* ps : GameState->GetPlayerArray()) {
            auto* sps = dynamic_cast<ALeonTournamentPlayerState*>(ps);
            if (sps && !sps->IsBot())
                ++n;
        }
        return n;
    }

    int32_t ALeonTournamentGameMode::CountBotsOnTeam(ELeonTournamentTeam InTeam) const {
        int32_t n = 0;
        if (!GameState)
            return 0;
        for (APlayerState* ps : GameState->GetPlayerArray()) {
            auto* sps = dynamic_cast<ALeonTournamentPlayerState*>(ps);
            if (sps && sps->IsBot() && sps->GetTeam() == InTeam)
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
        ps->SetCharacterSkin(RandomBotCharacterSkin());
        bot->SetPlayerState(ps);
        if (GameState)
            GameState->AddPlayerState(ps);
        World->AddAIController(bot);
        bot->SetWaypoints(Waypoints);
        bot->SetCoverPoints(CoverPoints);
        return bot;
    }

    void ALeonTournamentGameMode::ClearAllBots() {
        if (!World)
            return;
        std::vector<ALeonTournamentBotController*> bots;
        for (AAIController* ai : World->GetAIControllers()) {
            if (auto* bot = dynamic_cast<ALeonTournamentBotController*>(ai))
                bots.push_back(bot);
        }
        for (ALeonTournamentBotController* bot : bots) {
            APlayerState* ps = bot->GetPlayerState();
            APawn* pawn = bot->GetPawn();
            bot->UnPossess();
            if (pawn)
                World->DestroyActor(pawn);
            World->DestroyActor(bot);
            if (ps)
                World->DestroyActor(ps);
        }
        RefreshTeamCounts();
    }

    void ALeonTournamentGameMode::SyncLobbyBots() {
        ApplyMatchCapacityFromLobby();
        if (!World || World->GetNetMode() == ENetMode::Client)
            return;

        int32_t want1 = 2;
        int32_t want2 = 2;
        if (auto* gi = GetLeonTournamentGameInstance()) {
            want1 = gi->GetDesiredBotsTeam1();
            want2 = gi->GetDesiredBotsTeam2();
        }
        const int32_t humans = CountHumans();
        int32_t maxBots = std::max(0, Config.MaxPlayers - humans);
        while (want1 + want2 > maxBots && (want1 > 0 || want2 > 0)) {
            if (want1 >= want2 && want1 > 0)
                --want1;
            else if (want2 > 0)
                --want2;
            else
                break;
        }

        ClearAllBots();
        for (int32_t i = 0; i < want1; ++i) {
            char name[32];
            std::snprintf(name, sizeof(name), "Bot_T1_%d", NextBotId++);
            SpawnBot(ELeonTournamentTeam::Team1, name);
        }
        for (int32_t i = 0; i < want2; ++i) {
            char name[32];
            std::snprintf(name, sizeof(name), "Bot_T2_%d", NextBotId++);
            SpawnBot(ELeonTournamentTeam::Team2, name);
        }
        RefreshTeamCounts();
    }

    void ALeonTournamentGameMode::FillBotsToCapacity() {
        SyncLobbyBots();
    }

    void ALeonTournamentGameMode::BuildArena() {
        if (bArenaBuilt || !World)
            return;
        bArenaBuilt = true;

        constexpr float kHalf = 36.0f;
        constexpr float kWallH = 14.0f;
        const bool bHasGeometry = World->FindActorByName("Floor") != nullptr;

        if (!bHasGeometry) {
            FLeonTournamentArenaBuilder::SpawnBox(World, "Floor", {0.0f, -0.25f, 0.0f},
                                                  {kHalf * 2.0f, 0.5f, kHalf * 2.0f},
                                                  ELeonTournamentArenaSurface::Floor, {1.0f, 1.0f, 1.0f}, 8.0f);
            FLeonTournamentArenaBuilder::SpawnBox(World, "WallN", {0.0f, kWallH * 0.5f, -kHalf},
                                                  {kHalf * 2.0f, kWallH, 0.8f}, ELeonTournamentArenaSurface::Wall,
                                                  {1.0f, 1.0f, 1.0f}, 3.0f);
            FLeonTournamentArenaBuilder::SpawnBox(World, "WallS", {0.0f, kWallH * 0.5f, kHalf},
                                                  {kHalf * 2.0f, kWallH, 0.8f}, ELeonTournamentArenaSurface::Wall,
                                                  {1.0f, 1.0f, 1.0f}, 3.0f);
            FLeonTournamentArenaBuilder::SpawnBox(World, "WallW", {-kHalf, kWallH * 0.5f, 0.0f},
                                                  {0.8f, kWallH, kHalf * 2.0f}, ELeonTournamentArenaSurface::Wall,
                                                  {1.0f, 1.0f, 1.0f}, 3.0f);
            FLeonTournamentArenaBuilder::SpawnBox(World, "WallE", {kHalf, kWallH * 0.5f, 0.0f},
                                                  {0.8f, kWallH, kHalf * 2.0f}, ELeonTournamentArenaSurface::Wall,
                                                  {1.0f, 1.0f, 1.0f}, 3.0f);
        }

        if (FApplication::HasInstance()) {
            if (auto* renderer = World->GetWorldRenderer()) {
                renderer->ClearPlanarReflectionPlanes();
                renderer->AddPlanarReflectionPlane({0.0f, 1.0f, 0.0f}, 0.0f);
            }
        }

        if (!bHasGeometry) {
            auto maze = [&](const char* n, const glm::vec3& loc, const glm::vec3& sc) {
                FLeonTournamentArenaBuilder::SpawnBox(World, n, loc, sc, ELeonTournamentArenaSurface::Wall,
                                                      {0.92f, 0.92f, 0.95f}, 2.0f);
            };
            maze("MazeW_A", {-12.0f, 2.2f, -28.0f}, {0.8f, 4.4f, 8.0f});
            maze("MazeW_B", {-12.0f, 2.2f, -10.0f}, {0.8f, 4.4f, 12.0f});
            maze("MazeW_C", {-12.0f, 2.2f, 10.0f}, {0.8f, 4.4f, 12.0f});
            maze("MazeW_D", {-12.0f, 2.2f, 28.0f}, {0.8f, 4.4f, 8.0f});
            maze("MazeE_A", {12.0f, 2.2f, -28.0f}, {0.8f, 4.4f, 8.0f});
            maze("MazeE_B", {12.0f, 2.2f, -10.0f}, {0.8f, 4.4f, 12.0f});
            maze("MazeE_C", {12.0f, 2.2f, 10.0f}, {0.8f, 4.4f, 12.0f});
            maze("MazeE_D", {12.0f, 2.2f, 28.0f}, {0.8f, 4.4f, 8.0f});
            maze("MazeN_A", {-28.0f, 2.2f, -12.0f}, {8.0f, 4.4f, 0.8f});
            maze("MazeN_B", {-8.0f, 2.2f, -12.0f}, {14.0f, 4.4f, 0.8f});
            maze("MazeN_C", {8.0f, 2.2f, -12.0f}, {14.0f, 4.4f, 0.8f});
            maze("MazeN_D", {28.0f, 2.2f, -12.0f}, {8.0f, 4.4f, 0.8f});
            maze("MazeS_A", {-28.0f, 2.2f, 12.0f}, {8.0f, 4.4f, 0.8f});
            maze("MazeS_B", {-8.0f, 2.2f, 12.0f}, {14.0f, 4.4f, 0.8f});
            maze("MazeS_C", {8.0f, 2.2f, 12.0f}, {14.0f, 4.4f, 0.8f});
            maze("MazeS_D", {28.0f, 2.2f, 12.0f}, {8.0f, 4.4f, 0.8f});

            FLeonTournamentArenaBuilder::SpawnBox(World, "CoverA", {-22.0f, 1.15f, -22.0f}, {3.6f, 2.3f, 1.4f},
                                                  ELeonTournamentArenaSurface::Prop);
            FLeonTournamentArenaBuilder::SpawnBox(World, "CoverB", {22.0f, 1.15f, 22.0f}, {3.6f, 2.3f, 1.4f},
                                                  ELeonTournamentArenaSurface::Metal, {0.7f, 0.75f, 0.85f});
            FLeonTournamentArenaBuilder::SpawnBox(World, "CoverC", {-22.0f, 1.15f, 22.0f}, {1.6f, 2.3f, 3.6f},
                                                  ELeonTournamentArenaSurface::Accent);
            FLeonTournamentArenaBuilder::SpawnBox(World, "CoverD", {22.0f, 1.15f, -22.0f}, {1.6f, 2.3f, 3.6f},
                                                  ELeonTournamentArenaSurface::Accent);
            FLeonTournamentArenaBuilder::SpawnBox(World, "CoverMidW", {-4.0f, 1.15f, 0.0f}, {2.8f, 2.3f, 1.2f},
                                                  ELeonTournamentArenaSurface::Metal);
            FLeonTournamentArenaBuilder::SpawnBox(World, "CoverMidE", {4.0f, 1.15f, 0.0f}, {2.8f, 2.3f, 1.2f},
                                                  ELeonTournamentArenaSurface::Metal);
            FLeonTournamentArenaBuilder::SpawnBox(World, "CoverN", {0.0f, 1.15f, -20.0f}, {4.0f, 2.3f, 1.3f},
                                                  ELeonTournamentArenaSurface::Prop);
            FLeonTournamentArenaBuilder::SpawnBox(World, "CoverS", {0.0f, 1.15f, 20.0f}, {4.0f, 2.3f, 1.3f},
                                                  ELeonTournamentArenaSurface::Prop);
            FLeonTournamentArenaBuilder::SpawnBox(World, "PillarNW", {-18.0f, 2.5f, -18.0f}, {1.2f, 5.0f, 1.2f},
                                                  ELeonTournamentArenaSurface::Metal);
            FLeonTournamentArenaBuilder::SpawnBox(World, "PillarNE", {18.0f, 2.5f, -18.0f}, {1.2f, 5.0f, 1.2f},
                                                  ELeonTournamentArenaSurface::Metal);
            FLeonTournamentArenaBuilder::SpawnBox(World, "PillarSW", {-18.0f, 2.5f, 18.0f}, {1.2f, 5.0f, 1.2f},
                                                  ELeonTournamentArenaSurface::Metal);
            FLeonTournamentArenaBuilder::SpawnBox(World, "PillarSE", {18.0f, 2.5f, 18.0f}, {1.2f, 5.0f, 1.2f},
                                                  ELeonTournamentArenaSurface::Metal);
        }

        Team1Spawns = {{-28.0f, 2.0f, -24.0f}, {-28.0f, 2.0f, -8.0f}, {-28.0f, 2.0f, 8.0f}, {-28.0f, 2.0f, 24.0f},
                       {-24.0f, 2.0f, -24.0f}, {-24.0f, 2.0f, 24.0f}, {-30.0f, 2.0f, 0.0f}, {-20.0f, 2.0f, 0.0f},
                       {-26.0f, 2.0f, -14.0f}, {-26.0f, 2.0f, 14.0f}};
        Team2Spawns = {{28.0f, 2.0f, 24.0f}, {28.0f, 2.0f, 8.0f},   {28.0f, 2.0f, -8.0f}, {28.0f, 2.0f, -24.0f},
                       {24.0f, 2.0f, 24.0f}, {24.0f, 2.0f, -24.0f}, {30.0f, 2.0f, 0.0f},  {20.0f, 2.0f, 0.0f},
                       {26.0f, 2.0f, 14.0f}, {26.0f, 2.0f, -14.0f}};
        Waypoints = {{-24.0f, 2.0f, -24.0f}, {-24.0f, 2.0f, 0.0f},  {-24.0f, 2.0f, 24.0f}, {0.0f, 2.0f, -24.0f},
                     {0.0f, 2.0f, 0.0f},     {0.0f, 2.0f, 24.0f},   {24.0f, 2.0f, -24.0f}, {24.0f, 2.0f, 0.0f},
                     {24.0f, 2.0f, 24.0f},   {-12.0f, 2.0f, 0.0f},  {12.0f, 2.0f, 0.0f},   {0.0f, 2.0f, -12.0f},
                     {0.0f, 2.0f, 12.0f},    {-20.0f, 2.0f, -8.0f}, {20.0f, 2.0f, 8.0f},   {-8.0f, 2.0f, 20.0f},
                     {8.0f, 2.0f, -20.0f}};
        CoverPoints = {{-24.0f, 2.0f, -20.0f}, {-20.0f, 2.0f, -24.0f}, {24.0f, 2.0f, 20.0f},  {20.0f, 2.0f, 24.0f},
                       {-24.0f, 2.0f, 20.0f},  {-20.0f, 2.0f, 24.0f},  {24.0f, 2.0f, -20.0f}, {20.0f, 2.0f, -24.0f},
                       {-6.0f, 2.0f, 0.0f},    {6.0f, 2.0f, 0.0f},     {0.0f, 2.0f, -14.0f},  {0.0f, 2.0f, 14.0f},
                       {-8.0f, 2.0f, -8.0f},   {8.0f, 2.0f, 8.0f}};

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

        if (!World->FindActorByName("NavBounds")) {
            auto* navBounds = World->SpawnActor<ANavMeshBoundsVolume>("NavBounds");
            navBounds->SetActorLocation({0.0f, 1.0f, 0.0f});
            navBounds->SetActorScale({kHalf * 2.0f - 2.0f, 2.5f, kHalf * 2.0f - 2.0f});
            World->RebuildNavigation();
            if (auto* nav = World->GetNavigationSystem()) {
                LE_CORE_INFO("Arena nav: built={0} walkable={1} {2}x{3}", nav->IsBuilt() ? 1 : 0,
                             nav->GetWalkableCount(), nav->GetWidth(), nav->GetDepth());
            }
        }

        // Synchronous Lightmass bake freezes match start; use cached lightmaps if present.
        // Do not call EnsurePlayableLighting here — it destroys/rebuilds local lights and
        // invalidates LightmapBakeHash on authored maps (TournamentArenaNight).
        TryApplyCachedArenaLightmaps();

        if (World->FindActorByName("PU_Shotgun_A"))
            return;

        auto spawnWeaponPickup = [&](const char* name, ELeonTournamentWeaponId id, const glm::vec3& loc) {
            auto* p = World->SpawnActor<ALeonTournamentWeaponPickup>(name);
            if (!p)
                return;
            p->SetWeaponId(id);
            p->SetRespawnDelay(15.0f);
            p->SetAnchorLocation(loc);
        };
        auto spawnHealth = [&](const char* name, const glm::vec3& loc) {
            auto* p = World->SpawnActor<ALeonTournamentHealthPickup>(name);
            if (!p)
                return;
            p->SetRespawnDelay(15.0f);
            p->SetAnchorLocation(loc);
        };

        spawnWeaponPickup("PU_Shotgun_A", ELeonTournamentWeaponId::Shotgun, {-18.0f, 1.1f, 0.0f});
        spawnWeaponPickup("PU_Shotgun_B", ELeonTournamentWeaponId::Shotgun, {18.0f, 1.1f, 0.0f});
        spawnWeaponPickup("PU_Rocket_A", ELeonTournamentWeaponId::Rocket, {-8.0f, 1.1f, -18.0f});
        spawnWeaponPickup("PU_Rocket_B", ELeonTournamentWeaponId::Rocket, {28.0f, 1.1f, -12.0f});
        spawnWeaponPickup("PU_Laser_A", ELeonTournamentWeaponId::Laser, {0.0f, 1.1f, 18.0f});
        spawnWeaponPickup("PU_Laser_B", ELeonTournamentWeaponId::Laser, {0.0f, 1.1f, -18.0f});
        spawnWeaponPickup("PU_Grenade_A", ELeonTournamentWeaponId::Grenade, {-28.0f, 1.1f, 12.0f});
        spawnWeaponPickup("PU_Grenade_B", ELeonTournamentWeaponId::Grenade, {18.0f, 1.1f, -12.0f});
        spawnWeaponPickup("PU_Flamer_A", ELeonTournamentWeaponId::Flamethrower, {-16.0f, 1.1f, 8.0f});
        spawnWeaponPickup("PU_Flamer_B", ELeonTournamentWeaponId::Flamethrower, {16.0f, 1.1f, -8.0f});

        spawnHealth("PU_Health_A", {-16.0f, 1.0f, -16.0f});
        spawnHealth("PU_Health_B", {16.0f, 1.0f, 16.0f});
        spawnHealth("PU_Health_C", {-16.0f, 1.0f, 16.0f});
        spawnHealth("PU_Health_D", {16.0f, 1.0f, -16.0f});
        spawnHealth("PU_Health_E", {0.0f, 1.0f, 26.0f});
        spawnHealth("PU_Health_F", {0.0f, 1.0f, -26.0f});

        auto spawnJumpPad = [&](const char* name, const glm::vec3& loc, const glm::vec3& vel) {
            auto* p = World->SpawnActor<ALeonTournamentJumpPad>(name);
            if (!p)
                return;
            p->SetActorLocation(loc);
            p->SetPadVelocity(vel);
        };
        spawnJumpPad("JumpPad_Mid", {0.0f, 0.35f, 0.0f}, {0.0f, 9.0f, 0.0f});
        spawnJumpPad("JumpPad_North", {0.0f, 0.35f, 22.0f}, {0.0f, 8.0f, -4.0f});
        spawnJumpPad("JumpPad_South", {0.0f, 0.35f, -22.0f}, {0.0f, 8.0f, 4.0f});
        spawnJumpPad("JumpPad_West", {-22.0f, 0.35f, 0.0f}, {4.0f, 8.0f, 0.0f});
        spawnJumpPad("JumpPad_East", {22.0f, 0.35f, 0.0f}, {-4.0f, 8.0f, 0.0f});
    }

    void ALeonTournamentGameMode::EnsurePlayableLighting() {
        if (!World)
            return;

        // Day: directional + HDR only. Night (`TournamentArenaNight`): moonlight plus a
        // small Static point set (Lightmass) and a few Stationary spots. Always strip
        // leftover local lights / Ceiling first so StartMatch cannot stack a second grid.
        std::vector<AActor*> doomed;
        for (const auto& actor : World->GetAllActors()) {
            if (!actor)
                continue;
            const bool bLocalLight =
                actor->HasComponent<FPointLightComponent>() || actor->HasComponent<FSpotLightComponent>();
            if (bLocalLight || actor->GetName() == "Ceiling")
                doomed.push_back(actor.get());
        }
        for (AActor* actor : doomed)
            World->DestroyActor(actor);

        const bool bNight = IsNightArenaMap();

        AActor* env = World->FindActorByName("Environment Skybox");
        if (!env)
            env = World->SpawnActor<AActor>("Environment Skybox");
        FSkyboxComponent sky;
        sky.bEnabled = true;
        sky.bUseHDREnvironmentMap = true;
        if (bNight) {
            sky.Exposure = 0.88f;
            sky.SunIntensity = 0.35f;
            sky.EnvironmentIntensity = 0.95f;
            sky.HDREnvironmentMapPath = "/Game/HDR/NightSky1k.lhdr";
            sky.SkyZenithColor = {0.02f, 0.04f, 0.10f};
            sky.HorizonColor = {0.08f, 0.10f, 0.18f};
            sky.GroundColor = {0.03f, 0.03f, 0.04f};
            sky.SunColor = {0.55f, 0.65f, 0.95f};
        } else {
            sky.Exposure = 0.95f;
            sky.SunIntensity = 2.2f;
            sky.EnvironmentIntensity = 1.55f;
            sky.HDREnvironmentMapPath = "/Game/HDR/DaySky1k.lhdr";
            sky.SkyZenithColor = {0.12f, 0.28f, 0.55f};
            sky.HorizonColor = {0.55f, 0.62f, 0.75f};
            sky.GroundColor = {0.18f, 0.19f, 0.22f};
            sky.SunColor = {1.0f, 0.96f, 0.88f};
        }
        if (FApplication::HasInstance())
            sky.HDREnvironmentMap = UAssetManager::GetTexture2D(sky.HDREnvironmentMapPath);
        if (env->HasComponent<FSkyboxComponent>())
            env->GetComponent<FSkyboxComponent>() = sky;
        else
            env->AddComponent<FSkyboxComponent>(sky);

        FWorldSettingsComponent ws;
        ws.bStaticLighting = true;
        ws.LightingBuildQuality = ELightingBuildQuality::Preview;
        ws.LightmapResolution = 48;
        ws.NumIndirectBounces = 1;
        ws.SamplesPerTexel = 4;
        ws.IndirectIntensity = 1.1f;
        ws.bAmbientOcclusion = true;
        ws.LightmapAssetPath = ArenaLightmapPath();
        if (env->HasComponent<FWorldSettingsComponent>())
            env->GetComponent<FWorldSettingsComponent>() = ws;
        else
            env->AddComponent<FWorldSettingsComponent>(ws);

        bool bHasSun = false;
        for (const auto& actor : World->GetAllActors()) {
            if (actor && actor->HasComponent<FDirectionalLightComponent>()) {
                auto& sun = actor->GetComponent<FDirectionalLightComponent>();
                sun.bEnabled = true;
                sun.Mobility = ELightMobility::Stationary;
                sun.Light.Direction =
                    glm::normalize(glm::vec3(bNight ? 0.28f : -0.25f, -1.0f, bNight ? -0.32f : -0.35f));
                sun.Light.Color = bNight ? glm::vec3(0.45f, 0.58f, 0.95f) : glm::vec3(1.0f, 0.97f, 0.90f);
                sun.Light.Intensity = bNight ? 1.6f : 3.2f;
                bHasSun = true;
                break;
            }
        }
        if (!bHasSun) {
            AActor* sunActor = World->SpawnActor<AActor>(bNight ? "Moonlight" : "Directional Sunlight");
            sunActor->SetActorLocation({0.0f, 14.0f, 0.0f});
            FDirectionalLightComponent sun;
            sun.bEnabled = true;
            sun.Mobility = ELightMobility::Stationary;
            sun.Light.Direction = glm::normalize(glm::vec3(bNight ? 0.28f : -0.25f, -1.0f, bNight ? -0.32f : -0.35f));
            sun.Light.Color = bNight ? glm::vec3(0.45f, 0.58f, 0.95f) : glm::vec3(1.0f, 0.97f, 0.90f);
            sun.Light.Intensity = bNight ? 1.6f : 3.2f;
            sunActor->AddComponent<FDirectionalLightComponent>(sun);
        }

        if (bNight)
            SpawnNightArenaLights(World);
    }

    void ALeonTournamentGameMode::TryApplyCachedArenaLightmaps() {
        if (bArenaLightingBaked || !World)
            return;
        namespace fs = std::filesystem;
        const fs::path lmAbs =
            fs::path(UAssetManager::GetContentRoot()) / "Lightmaps" / fs::path(ArenaLightmapPath()).filename();
        const fs::path mapAbs = fs::path(UAssetManager::GetContentRoot()) / "Maps" / ArenaMapFileName();
        if (!fs::exists(lmAbs) || !fs::exists(mapAbs))
            return;

        auto stamped = UWorld::Create("ArenaLightmapRead");
        FMapSerializer reader(stamped);
        if (!reader.Deserialize(mapAbs.string()))
            return;

        for (const auto& bakedRef : stamped->GetAllActors()) {
            if (!bakedRef || !bakedRef->HasComponent<FMeshComponent>())
                continue;
            AActor* liveActor = World->FindActorByName(bakedRef->GetName());
            if (!liveActor || !liveActor->HasComponent<FMeshComponent>())
                continue;
            auto& src = bakedRef->GetComponent<FMeshComponent>();
            auto& dst = liveActor->GetComponent<FMeshComponent>();
            dst.LightmapAssetPath = src.LightmapAssetPath.empty() ? ArenaLightmapPath() : src.LightmapAssetPath;
            dst.LightmapIndex = src.LightmapIndex;
            dst.LightmapScale = src.LightmapScale;
            dst.LightmapBias = src.LightmapBias;
        }
        bArenaLightingBaked = true;
        LE_CORE_INFO("Applied cached arena lightmaps from {0}", lmAbs.string());
    }

    void ALeonTournamentGameMode::TryBakeArenaLighting() {
        // Opt-in only: set LEON_BAKE_ARENA=1 to rebuild lightmaps (blocks StartMatch for several seconds).
        const char* flag = std::getenv("LEON_BAKE_ARENA");
        if (!flag || flag[0] != '1')
            return;
        if (bArenaLightingBaked || !World || !UEngine::HasInstance())
            return;
        auto live = UEngine::Get().GetWorld();
        if (!live)
            return;

        namespace fs = std::filesystem;
        const fs::path mapAbs = fs::path(UAssetManager::GetContentRoot()) / "Maps" / ArenaMapFileName();
        std::error_code ec;
        fs::create_directories(mapAbs.parent_path(), ec);

        FMapSerializer serializer(live);
        if (!serializer.Serialize(mapAbs.string())) {
            LE_CORE_WARN("Arena bake: failed to serialize {0}", mapAbs.string());
            return;
        }

        FLightmassSettings settings;
        ApplyLightingBuildQuality(ELightingBuildQuality::Preview, settings);
        const auto result = FLightmass::BakeMap(mapAbs.string(), settings, true);
        if (!result.bSuccess) {
            LE_CORE_WARN("Arena bake skipped: {0}", result.Message);
            return;
        }

        auto stamped = UWorld::Create("ArenaBakeRead");
        FMapSerializer reader(stamped);
        if (!reader.Deserialize(mapAbs.string())) {
            LE_CORE_WARN("Arena bake: could not re-read stamped map");
            return;
        }

        for (const auto& bakedRef : stamped->GetAllActors()) {
            if (!bakedRef || !bakedRef->HasComponent<FMeshComponent>())
                continue;
            AActor* liveActor = World->FindActorByName(bakedRef->GetName());
            if (!liveActor || !liveActor->HasComponent<FMeshComponent>())
                continue;
            auto& src = bakedRef->GetComponent<FMeshComponent>();
            auto& dst = liveActor->GetComponent<FMeshComponent>();
            dst.LightmapAssetPath = src.LightmapAssetPath;
            dst.LightmapIndex = src.LightmapIndex;
            dst.LightmapScale = src.LightmapScale;
            dst.LightmapBias = src.LightmapBias;
            if (dst.LightmapAssetPath.empty())
                dst.LightmapAssetPath = ArenaLightmapPath();
        }
        if (AActor* envLive = World->FindActorByName("Environment Skybox")) {
            if (envLive->HasComponent<FWorldSettingsComponent>()) {
                auto& liveWs = envLive->GetComponent<FWorldSettingsComponent>();
                liveWs.LightmapBakeHash = result.BakeHash;
                liveWs.LightmapAssetPath = ArenaLightmapPath();
            }
        }
        bArenaLightingBaked = true;
        LE_CORE_INFO("Arena lightmap bake ok atlas={0}x{1} meshes={2}", result.AtlasWidth, result.AtlasHeight,
                     result.StaticMeshCount);
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
        if (ps) {
            if (!bBot)
                SyncLocalPlayerCharacterSkin(ps);
            pawn->ApplyCharacterSkin(ps->GetCharacterSkin());
        }
        pawn->SetActorLocation(spawn);
        pawn->SetFloorZ(0.0f);
        NewPlayer->Possess(pawn);
        if (!bBot)
            ApplyCameraPreference(pawn);
        FaceIntoArena(*pawn, team);
        if (auto* bot = dynamic_cast<ALeonTournamentBotController*>(NewPlayer)) {
            bot->SetWaypoints(Waypoints);
            bot->SetCoverPoints(CoverPoints);
            bot->NotifyRespawned();
        }
        // BeginPlay (and EnsureWeapon) may still be deferred during UWorld::StartPlay.
        if (pawn->ShouldSpawnWeapon())
            pawn->EnsureWeapon();
        ValidateSpawnedCharacter(*pawn, team);
    }

    void ALeonTournamentGameMode::PlaceMenuShowcase(ALeonTournamentCharacter& InCharacter) {
        InCharacter.SetMenuShowcase(true);
        ELeonTournamentCharacterSkin skin = ELeonTournamentCharacterSkin::YBot;
        if (auto* gi = GetLeonTournamentGameInstance())
            skin = gi->GetSelectedCharacterSkin();
        InCharacter.ApplyCharacterSkin(skin);
        RefreshMenuShowcasePlacement();
    }

    void ALeonTournamentGameMode::RefreshMenuShowcasePlacement() {
        if (!ShowcaseCharacter || ShowcaseCharacter->IsPendingKill())
            return;

        float vpW = FUILayout::kDesignWidth;
        float vpH = FUILayout::kDesignHeight;
        if (FApplication::HasInstance()) {
            const auto& window = FApplication::Get().GetWindow();
            if (window.GetWidth() > 0 && window.GetHeight() > 0) {
                vpW = static_cast<float>(window.GetWidth());
                vpH = static_cast<float>(window.GetHeight());
            }
        }

        FPerspectiveCamera cam(45.0f, std::max(vpW, 1.0f) / std::max(vpH, 1.0f), 0.1f, 1000.0f);
        cam.SetPosition(kLeonTournamentMenuCameraPosition);
        cam.SetRotation(kLeonTournamentMenuCameraPitchDeg, kLeonTournamentMenuCameraYawDeg);
        cam.SetViewportSize(static_cast<uint32_t>(std::max(vpW, 1.0f)), static_cast<uint32_t>(std::max(vpH, 1.0f)));
        if (World) {
            if (auto* pc = World->GetFirstPlayerController()) {
                if (auto* mgr = pc->GetPlayerCameraManager())
                    mgr->GetCamera() = cam;
            }
        }

        const bool bLobby = GetGameState() && GetGameState()->GetMatchState() == ELeonTournamentMatchState::Lobby;
        const float panelPx = LeonTournamentMenuPanelWidthPx(vpW, bLobby);
        const glm::vec3 loc = LeonTournamentMenuShowcaseLocation(
            cam.GetPosition(), cam.GetForwardDirection(), cam.GetRightDirection(), cam.GetFOV(), cam.GetAspectRatio(),
            ShowcaseCharacter->GetCapsuleHalfHeight(), vpW, panelPx);
        ShowcaseCharacter->SetActorLocation(loc);
        ShowcaseCharacter->SetFloorZ(0.0f);
        ShowcaseCharacter->SetControlYaw(90.0f);
        ShowcaseCharacter->SetControlPitch(0.0f);
        ShowcaseCharacter->ApplyYawOnlyActorRotation();
        if (ShowcaseFloor && !ShowcaseFloor->IsPendingKill())
            ShowcaseFloor->SetActorLocation({loc.x, -0.05f, loc.z});
    }

    void ALeonTournamentGameMode::EnsureMenuShowcase() {
        // Flow: menu / lobby character preview
        // 1. Drive the unpossessed PlayerCameraManager to the mid-torso menu view.
        // 2. Stand the pawn on the floor in the frustum gap right of the UI panel.
        // 3. Apply the GameInstance skin; cycling calls NotifySelectedCharacterChanged.
        if (!World)
            return;
        if (!ShowcaseFloor || ShowcaseFloor->IsPendingKill()) {
            ShowcaseFloor = World->FindActorByName(kLeonTournamentMenuShowcaseFloorName);
            if (!ShowcaseFloor) {
                ShowcaseFloor = FLeonTournamentArenaBuilder::SpawnBox(World, kLeonTournamentMenuShowcaseFloorName,
                                                                      {0.0f, -0.05f, 7.2f}, {10.0f, 0.1f, 10.0f},
                                                                      ELeonTournamentArenaSurface::Floor);
            }
        }
        if (!ShowcaseCharacter || ShowcaseCharacter->IsPendingKill()) {
            ShowcaseCharacter =
                dynamic_cast<ALeonTournamentCharacter*>(World->FindActorByName(kLeonTournamentMenuShowcaseActorName));
            if (!ShowcaseCharacter)
                ShowcaseCharacter = World->SpawnActor<ALeonTournamentCharacter>(kLeonTournamentMenuShowcaseActorName);
        }
        if (ShowcaseCharacter)
            PlaceMenuShowcase(*ShowcaseCharacter);
    }

    void ALeonTournamentGameMode::DestroyMenuShowcase() {
        if (World) {
            if (ShowcaseCharacter && !ShowcaseCharacter->IsPendingKill())
                World->DestroyActor(ShowcaseCharacter);
            else if (AActor* leftover = World->FindActorByName(kLeonTournamentMenuShowcaseActorName))
                World->DestroyActor(leftover);
            if (ShowcaseFloor && !ShowcaseFloor->IsPendingKill())
                World->DestroyActor(ShowcaseFloor);
            else if (AActor* leftover = World->FindActorByName(kLeonTournamentMenuShowcaseFloorName))
                World->DestroyActor(leftover);
        }
        ShowcaseCharacter = nullptr;
        ShowcaseFloor = nullptr;
    }

    void ALeonTournamentGameMode::NotifySelectedCharacterChanged() {
        if (auto* pc = World ? World->GetFirstPlayerController() : nullptr) {
            if (auto* ps = dynamic_cast<ALeonTournamentPlayerState*>(pc->GetPlayerState()))
                SyncLocalPlayerCharacterSkin(ps);
        }
        auto* gi = GetLeonTournamentGameInstance();
        if (ShowcaseCharacter && !ShowcaseCharacter->IsPendingKill() && gi)
            ShowcaseCharacter->ApplyCharacterSkin(gi->GetSelectedCharacterSkin());
    }

    void ALeonTournamentGameMode::ApplyCameraPreference(ALeonTournamentCharacter* InCharacter) {
        if (!InCharacter)
            return;
        InCharacter->SetThirdPerson(bPreferThirdPerson);
        InCharacter->SetFloorZ(0.0f);
    }

    void ALeonTournamentGameMode::RequestStartMatch() {
        if (World && World->GetNetMode() == ENetMode::Client)
            return;
        StartMatch();
    }

    void ALeonTournamentGameMode::OpenAnimLab() {
        if (!IsNetworkAuthority() || !UEngine::HasInstance())
            return;
        DestroyMenuShowcase();
        SetTravelGameModeClass("ALeonTournamentAnimLabGameMode");
        if (World)
            UGameplayStatics::OpenLevel(World, "/Game/Maps/AnimLab");
    }

    void ALeonTournamentGameMode::OpenNightArena() {
        if (!IsNetworkAuthority() || !UEngine::HasInstance())
            return;
        DestroyMenuShowcase();
        SetTravelGameModeClass("ALeonTournamentGameMode");
        if (World)
            UGameplayStatics::OpenLevel(World, "/Game/Maps/TournamentArenaNight");
    }

    void ALeonTournamentGameMode::OpenPlayableMap(ELeonTournamentPlayableMap InMap) {
        if (!IsNetworkAuthority() || !UEngine::HasInstance())
            return;
        if (InMap == ELeonTournamentPlayableMap::Arena) {
            StartMatch();
            return;
        }
        DestroyMenuShowcase();
        SetTravelGameModeClass("ALeonTournamentGameMode");
        if (World)
            UGameplayStatics::OpenLevel(World, LeonTournamentPlayableMapPath(InMap));
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
        DestroyMenuShowcase();

        if (IsAuthoredPlayableMap() || WorldHasImportedStaticMeshes(World)) {
            // Authored maps (Night/Orbital): keep baked lights/geometry; only seed gameplay overlay
            // (team starts, pickups, nav, planar floor) via BuildArena's existing Floor-present path.
            BuildArena();
            if (World && !World->FindActorByName("NavBounds")) {
                glm::vec3 minB(1e9f), maxB(-1e9f);
                bool bAny = false;
                for (const auto& actor : World->GetAllActors()) {
                    if (!actor || !actor->HasComponent<FStaticMeshComponent>())
                        continue;
                    const auto& smc = actor->GetComponent<FStaticMeshComponent>();
                    if (!smc.StaticMesh)
                        continue;
                    const glm::mat4 worldXf = actor->GetActorWorldMatrix();
                    const glm::vec3& mn = smc.StaticMesh->GetBoundsMin();
                    const glm::vec3& mx = smc.StaticMesh->GetBoundsMax();
                    for (int i = 0; i < 8; ++i) {
                        glm::vec3 c((i & 1) ? mx.x : mn.x, (i & 2) ? mx.y : mn.y, (i & 4) ? mx.z : mn.z);
                        glm::vec3 w = glm::vec3(worldXf * glm::vec4(c, 1.0f));
                        minB = glm::min(minB, w);
                        maxB = glm::max(maxB, w);
                        bAny = true;
                    }
                }
                if (bAny) {
                    auto* navBounds = World->SpawnActor<ANavMeshBoundsVolume>("NavBounds");
                    navBounds->SetActorLocation((minB + maxB) * 0.5f);
                    glm::vec3 extent = maxB - minB;
                    navBounds->SetActorScale(
                        {std::max(extent.x, 4.0f), std::max(extent.y, 4.0f), std::max(extent.z, 4.0f)});
                    World->RebuildNavigation();
                }
            }
        } else {
            BuildArena();
            EnsurePlayableLighting();
        }
        NextTeam1Spawn = 0;
        NextTeam2Spawn = 0;
        if (World) {
            if (auto* pc = World->GetFirstPlayerController()) {
                if (auto* ps = dynamic_cast<ALeonTournamentPlayerState*>(pc->GetPlayerState())) {
                    if (ps->GetTeam() == ELeonTournamentTeam::None)
                        ps->SetTeam(AssignTeam());
                    ps->SetIsBot(false);
                    SyncLocalPlayerCharacterSkin(ps);
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
        if (World)
            World->GetTimerManager().ClearTimer(CountdownHandle);
        if (Config.StartCountdownSeconds <= 0.0f) {
            if (gs) {
                gs->SetMatchState(ELeonTournamentMatchState::Playing);
                gs->SetCountdownRemaining(0.0f);
            }
            StartingRemaining = 0.0f;
        } else if (World) {
            World->GetTimerManager().SetTimer(
                CountdownHandle,
                [this]() {
                    if (auto* match = GetGameState()) {
                        match->SetMatchState(ELeonTournamentMatchState::Playing);
                        match->SetCountdownRemaining(0.0f);
                    }
                },
                Config.StartCountdownSeconds, false);
        }
        PossessHumanPawns();

        if (World) {
            for (AAIController* ai : World->GetAIControllers()) {
                if (auto* bot = dynamic_cast<ALeonTournamentBotController*>(ai))
                    RestartPlayer(bot);
            }
        }
        RefreshTeamCounts();
        DamageLog.clear();
        RespawnTimerHandles.clear();
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
        return FLeonTournamentDamageRules::CanDamage(Config, InInstigator, InTarget);
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
        if (World) {
            if (AController* ctrl = InVictim.GetController()) {
                FTimerHandle handle;
                World->GetTimerManager().SetTimer(
                    handle,
                    [this, ctrl]() {
                        if (ctrl && !ctrl->IsPendingKill())
                            RestartPlayer(ctrl);
                        RespawnTimerHandles.erase(ctrl);
                    },
                    Config.RespawnDelaySeconds, false);
                RespawnTimerHandles[ctrl] = handle;
            }
        }

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
        if (World) {
            auto it = RespawnTimerHandles.find(ctrl);
            if (it != RespawnTimerHandles.end()) {
                World->GetTimerManager().ClearTimer(it->second);
                RespawnTimerHandles.erase(it);
            }
        }
        RestartPlayer(ctrl);
    }

    void ALeonTournamentGameMode::TickMatch(float DeltaSeconds) {
        auto* gs = GetGameState();
        if (!gs)
            return;
        if (gs->GetMatchState() == ELeonTournamentMatchState::Starting) {
            if (World && World->GetTimerManager().IsTimerActive(CountdownHandle))
                StartingRemaining = std::max(0.0f, World->GetTimerManager().GetTimerRemaining(CountdownHandle));
            else
                StartingRemaining = 0.0f;
            gs->SetCountdownRemaining(StartingRemaining);
            if (StartingRemaining > 0.0f)
                return;
            gs->SetMatchState(ELeonTournamentMatchState::Playing);
            gs->SetCountdownRemaining(0.0f);
        }
        if (gs->GetMatchState() != ELeonTournamentMatchState::Playing)
            return;

        if (Config.MatchDurationSeconds > 0.0f) {
            gs->SetRemainingTime(std::max(0.0f, gs->GetRemainingTime() - DeltaSeconds));
            if (gs->GetRemainingTime() <= 0.0f) {
                if (gs->GetTeam1Kills() > gs->GetTeam2Kills())
                    EndMatch(ELeonTournamentMatchWinner::Team1);
                else if (gs->GetTeam2Kills() > gs->GetTeam1Kills())
                    EndMatch(ELeonTournamentMatchWinner::Team2);
                else
                    EndMatch(ELeonTournamentMatchWinner::Draw);
            }
        }
    }

    void ALeonTournamentGameMode::TickAutoPlay(float DeltaSeconds) {
        auto* gi = GetLeonTournamentGameInstance();
        if (!gi || !gi->IsAutoOfflineMatch() || bAutoPlayFinished)
            return;
        if (!bAutoPlayCollectorReady) {
            FFrameStatsCollector::Reset();
            FFrameStatsCollector::MarkMemoryBegin();
            bAutoPlayCollectorReady = true;
        }
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
        FFrameStatsCollector::MarkMemoryEnd();
        const FFrameCaptureSummary capture = FFrameStatsCollector::Compute(3.0f);
        const float avgMs =
            capture.SampleCount > 0
                ? capture.Frame.Average
                : (AutoPlaySamples > 0 ? AutoPlayMsSum / static_cast<float>(AutoPlaySamples) : timing.FrameMs);
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
            out << "min_frame_ms=" << (capture.SampleCount > 0 ? capture.Frame.Min : timing.FrameMs) << "\n";
            out << "max_frame_ms=" << (capture.SampleCount > 0 ? capture.Frame.Max : AutoPlayMsMax) << "\n";
            out << "median_frame_ms=" << capture.Frame.Median << "\n";
            out << "p1_frame_ms=" << capture.Frame.P1 << "\n";
            out << "p5_frame_ms=" << capture.Frame.P5 << "\n";
            out << "p95_frame_ms=" << capture.Frame.P95 << "\n";
            out << "p99_frame_ms=" << capture.Frame.P99 << "\n";
            out << "samples=" << capture.SampleCount << "\n";
            out << "spikes=" << capture.SpikeCount << "\n";
            out << "bound_class=" << FFrameStatsCollector::BoundClassName(capture.BoundClass) << "\n";
            out << "avg_cpu_work_ms=" << capture.AvgCPUWorkMs << "\n";
            out << "avg_gpu_ms=" << capture.AvgGPUMs << "\n";
            out << "avg_present_ms=" << capture.AvgPresentMs << "\n";
            out << "avg_game_ms=" << capture.Game.Average << "\n";
            out << "avg_render_ms=" << capture.Render.Average << "\n";
            out << "avg_shadow_ms=" << capture.Shadow.Average << "\n";
            out << "avg_opaque_ms=" << capture.Opaque.Average << "\n";
            out << "avg_planar_ms=" << capture.Planar.Average << "\n";
            out << "avg_postprocess_ms=" << capture.PostProcess.Average << "\n";
            out << "avg_anim_ms=" << capture.Animation.Average << "\n";
            out << "avg_characters_ms=" << capture.Characters.Average << "\n";
            out << "avg_overlaps_ms=" << capture.Overlaps.Average << "\n";
            out << "avg_controllers_ms=" << capture.Controllers.Average << "\n";
            out << "avg_ai_ms=" << capture.AI.Average << "\n";
            out << "avg_nav_ms=" << capture.Navigation.Average << "\n";
            out << "avg_physics_ms=" << capture.Physics.Average << "\n";
            out << "avg_ui_ms=" << capture.UI.Average << "\n";
            out << "avg_tick_actors=" << capture.AvgTickActors << "\n";
            out << "avg_tick_components=" << capture.AvgTickComponents << "\n";
            out << "avg_actors=" << capture.AvgActors << "\n";
            out << "avg_shadow_draws=" << capture.AvgShadowDraws << "\n";
            out << "avg_path_requests=" << capture.AvgPathRequests << "\n";
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
            out << "\n";
            out << FFrameStatsCollector::FormatReport(capture);
        }
        LE_CORE_INFO("AutoPlay report written to {}  avg={:.1f} ms ({:.0f} FPS)", path, avgMs, avgFps);
        if (FApplication::HasInstance())
            FApplication::Get().Close();
    }

    void ALeonTournamentGameMode::EndPlay() {
        DestroyMenuShowcase();
        DamageLog.clear();
        RespawnTimerHandles.clear();
        if (World) {
            World->GetTimerManager().ClearTimer(CountdownHandle);
            World->GetTimerManager().ClearTimer(PendingStartMatchHandle);
        }
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
        if (auto* gs = GetGameState()) {
            if (gs->GetMatchState() == ELeonTournamentMatchState::MainMenu ||
                gs->GetMatchState() == ELeonTournamentMatchState::Lobby)
                RefreshMenuShowcasePlacement();
        }
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
