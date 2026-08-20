#include <doctest/doctest.h>
#include "Core/FConfigFile.hpp"
#include "Core/FTimestep.hpp"
#include "Gameplay/AActor.hpp"
#include "Gameplay/ACameraActor.hpp"
#include "Gameplay/ADefaultPawn.hpp"
#include "Gameplay/AGameModeBase.hpp"
#include "Gameplay/AGameMode.hpp"
#include "Gameplay/AGameStateBase.hpp"
#include "Gameplay/AGameState.hpp"
#include "Gameplay/APawn.hpp"
#include "Gameplay/APlayerCameraManager.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Gameplay/APlayerState.hpp"
#include "Gameplay/UClassRegistry.hpp"
#include "Gameplay/UObject.hpp"
#include "Engine/Components.hpp"
#include "Engine/FMapSerializer.hpp"
#include "Engine/UEngine.hpp"
#include "Gameplay/APlayerStart.hpp"
#include "Gameplay/ACharacter.hpp"
#include "Gameplay/AProjectile.hpp"
#include "Gameplay/UProjectileMovementComponent.hpp"
#include "Assets/UAssetManager.hpp"
#include "Assets/UStaticMesh.hpp"
#include "Core/FProjectDescriptor.hpp"
#include "Core/FProjectPaths.hpp"

#include <filesystem>
#include <fstream>

namespace Leon {

    // Custom test actor to verify lifecycle callbacks
    class ATestLifecycleActor : public AActor {
    public:
        ATestLifecycleActor(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "TestActor")
            : AActor(InHandle, InWorld, InName) {}

        void PostInitializeComponents() override { bPostInitCalled = true; }
        void BeginPlay() override { bBeginPlayCalled = true; }
        void Tick(float DeltaSeconds) override {
            bTickCalled = true;
            TotalDelta += DeltaSeconds;
        }
        void EndPlay() override { bEndPlayCalled = true; }

        bool bPostInitCalled = false;
        bool bBeginPlayCalled = false;
        bool bTickCalled = false;
        bool bEndPlayCalled = false;
        float TotalDelta = 0.0f;
    };

    TEST_SUITE("Unreal Gameplay Framework - Architecture, Naming & Lifecycle Invariants") {

        TEST_CASE("1. UObject identity") {
            auto obj = std::make_shared<UObject>("EngineCoreObject");
            CHECK(obj->GetName() == "EngineCoreObject");

            obj->SetName("RenamedCoreObject");
            CHECK(obj->GetName() == "RenamedCoreObject");
        }

        TEST_CASE("2. UClass registration & factory") {
            auto& registry = UClassRegistry::Get();

            CHECK(registry.HasClass("AActor"));
            CHECK(registry.HasClass("APawn"));
            CHECK(registry.HasClass("ADefaultPawn"));
            CHECK(registry.HasClass("APlayerController"));
            CHECK(registry.HasClass("APlayerState"));
            CHECK(registry.HasClass("AGameStateBase"));
            CHECK(registry.HasClass("AGameState"));
            CHECK(registry.HasClass("AGameModeBase"));
            CHECK(registry.HasClass("AGameMode"));
            CHECK(registry.HasClass("ACameraActor"));
            CHECK(registry.HasClass("APlayerCameraManager"));
            CHECK(registry.HasClass("AHUD"));
            CHECK(registry.HasClass("ABlockingVolume"));
            CHECK(registry.HasClass("APhysicsVolume"));
            CHECK_FALSE(registry.HasClass("Actor"));
            CHECK_FALSE(registry.HasClass("HUD"));

            registry.RegisterClass<ATestLifecycleActor>("ATestLifecycleActor");
            CHECK(registry.HasClass("ATestLifecycleActor"));

            auto testWorld = UWorld::Create("RegistryTestWorld");
            AActor* customActor = registry.CreateActorOfClass("ATestLifecycleActor", testWorld.get(), "SpawnedCustom");
            REQUIRE(customActor != nullptr);
            CHECK(customActor->GetName() == "SpawnedCustom");
            CHECK(dynamic_cast<ATestLifecycleActor*>(customActor) != nullptr);
        }

        TEST_CASE("3. UWorld creation & naming") {
            auto world = UWorld::Create("Universe");
            REQUIRE(world != nullptr);
            CHECK(world->GetName() == "Universe");
            CHECK(!world->HasBegunPlay());
            CHECK(world->GetAllActors().empty());
        }

        TEST_CASE("4. SpawnActor & DestroyActor") {
            auto world = UWorld::Create("SpawnWorld");
            auto* actor = world->SpawnActor<AActor>("WorldActor");
            REQUIRE(actor != nullptr);
            CHECK(world->GetAllActors().size() == 1);
            CHECK(world->FindActorByName("WorldActor") == actor);

            actor->SetActorLocation({1.0f, 2.0f, 3.0f});
            CHECK(actor->GetActorLocation().x == doctest::Approx(1.0f));

            world->DestroyActor(actor);
            CHECK(world->GetAllActors().empty());
            CHECK(world->FindActorByName("WorldActor") == nullptr);
        }

        TEST_CASE("5. AGameModeBase defaults") {
            auto world = UWorld::Create("GameModeDefaultsWorld");
            auto gm = world->SpawnActor<AGameModeBase>("GameMode");
            REQUIRE(gm != nullptr);

            CHECK(gm->DefaultPawnClass == "ADefaultPawn");
            CHECK(gm->PlayerControllerClass == "APlayerController");
            CHECK(gm->HUDClass == "AHUD");
            CHECK(gm->GameStateClass == "AGameStateBase");
            CHECK(gm->PlayerStateClass == "APlayerState");
            CHECK(gm->DefaultSpawnLocation.y == doctest::Approx(3.5f));
        }

        TEST_CASE("5b. AGameMode defaults to AGameState") {
            auto world = UWorld::Create("GameModeWorld");
            auto gm = world->SpawnActor<AGameMode>("GameMode");
            REQUIRE(gm != nullptr);
            CHECK(gm->GameStateClass == "AGameState");
        }

        TEST_CASE("6. AGameStateBase creation & player registration") {
            auto world = UWorld::Create("GameStateWorld");
            auto gs = world->SpawnActor<AGameStateBase>("GameState");
            REQUIRE(gs != nullptr);

            auto ps1 = world->SpawnActor<APlayerState>("PS1");
            ps1->SetPlayerName("PlayerOne");
            gs->AddPlayerState(ps1);

            CHECK(gs->GetPlayerArray().size() == 1);
            CHECK(gs->GetPlayerArray()[0]->GetPlayerName() == "PlayerOne");
        }

        TEST_CASE("6c. GameState PlayerArray sorted by PlayerState Score") {
            auto world = UWorld::Create("ScoreboardWorld");
            auto gs = world->SpawnActor<AGameStateBase>("GameState");
            auto low = world->SpawnActor<APlayerState>("PSLow");
            auto high = world->SpawnActor<APlayerState>("PSHigh");
            low->SetPlayerName("Low");
            high->SetPlayerName("High");
            low->SetScore(1.0f);
            high->SetScore(10.0f);
            gs->AddPlayerState(low);
            gs->AddPlayerState(high);
            auto ranked = gs->GetPlayerArraySortedByScore();
            REQUIRE(ranked.size() == 2);
            CHECK(ranked[0] == high);
            CHECK(ranked[1] == low);
            CHECK(ranked[0]->GetScore() == doctest::Approx(10.0f));
        }

        TEST_CASE("6b. AGameMode StartMatch syncs remaining time on AGameState") {
            auto world = UWorld::Create("MatchClockWorld");
            auto* gm = world->SpawnActor<AGameMode>("GM");
            world->SetGameMode(gm);
            world->InitWorld();
            REQUIRE(gm->GetGameState());
            CHECK(gm->GetGameState()->GetMatchState() == EMatchState::WaitingToStart);
            CHECK_FALSE(gm->HasMatchStarted());
            CHECK_FALSE(gm->HasMatchInProgress());
            CHECK_FALSE(gm->PlayerCanRestart(nullptr));

            gm->GetGameState()->SetRemainingTime(10.0f);
            gm->StartMatch();
            CHECK(gm->HasMatchStarted());
            CHECK(gm->HasMatchInProgress());
            CHECK(gm->PlayerCanRestart(nullptr));
            CHECK_FALSE(gm->HasMatchEnded());
            CHECK(gm->GetGameState()->GetMatchState() == EMatchState::InProgress);

            world->BeginPlay();
            world->Tick(FTimestep(0.1f));
            CHECK(gm->GetGameState()->GetRemainingTime() == doctest::Approx(9.9f).epsilon(0.01f));

            gm->EndMatch();
            CHECK(gm->HasMatchEnded());
            CHECK_FALSE(gm->HasMatchInProgress());
            CHECK_FALSE(gm->PlayerCanRestart(nullptr));
            CHECK(gm->GetGameState()->GetMatchState() == EMatchState::WaitingPostMatch);

            gm->RestartGame();
            CHECK(gm->HasMatchInProgress());
            CHECK(gm->GetGameState()->GetMatchState() == EMatchState::InProgress);
        }

        TEST_CASE("7. APlayerController creation") {
            auto world = UWorld::Create("PCWorld");
            auto pc = world->SpawnActor<APlayerController>("PC");
            REQUIRE(pc != nullptr);

            world->AddPlayerController(pc);
            CHECK(world->GetFirstPlayerController() == pc);
        }

        TEST_CASE("8. APlayerState creation & metadata") {
            auto world = UWorld::Create("PSWorld");
            auto ps = world->SpawnActor<APlayerState>("PlayerState");
            REQUIRE(ps != nullptr);

            ps->SetPlayerName("Hero");
            ps->SetPlayerId(42);
            ps->SetScore(100.0f);

            CHECK(ps->GetPlayerName() == "Hero");
            CHECK(ps->GetPlayerId() == 42);
            CHECK(ps->GetScore() == doctest::Approx(100.0f));
        }

        TEST_CASE("9. DefaultPawn spawning & 10. Pawn possession") {
            auto world = UWorld::Create("PossessionWorld");
            auto pc = world->SpawnActor<APlayerController>("PC");
            auto pawn = world->SpawnActor<ADefaultPawn>("Pawn");

            REQUIRE(pc != nullptr);
            REQUIRE(pawn != nullptr);
            CHECK(!pawn->IsControlled());

            pc->Possess(pawn);
            CHECK(pawn->IsControlled());
            CHECK(pawn->GetController() == pc);
            CHECK(pc->GetPawn() == pawn);

            pc->UnPossess();
            CHECK(!pawn->IsControlled());
            CHECK(pc->GetPawn() == nullptr);
        }

        TEST_CASE("10b. Possess steals pawn from the previous controller") {
            auto world = UWorld::Create("StealPossessWorld");
            auto* pcA = world->SpawnActor<APlayerController>("PCA");
            auto* pcB = world->SpawnActor<APlayerController>("PCB");
            auto* pawn = world->SpawnActor<ADefaultPawn>("Pawn");
            REQUIRE(pcA);
            REQUIRE(pcB);
            REQUIRE(pawn);

            pcA->Possess(pawn);
            CHECK(pcA->GetPawn() == pawn);
            CHECK(pawn->GetController() == pcA);

            pcB->Possess(pawn);
            CHECK(pcA->GetPawn() == nullptr);
            CHECK(pcB->GetPawn() == pawn);
            CHECK(pawn->GetController() == pcB);
            CHECK(pcA->IsPlayerController());
        }

        TEST_CASE("11. PlayerCameraManager creation") {
            auto world = UWorld::Create("PCMWorld");
            auto pc = world->SpawnActor<APlayerController>("PC");
            world->AddPlayerController(pc);

            APlayerCameraManager* pcm = pc->GetPlayerCameraManager();
            REQUIRE(pcm != nullptr);
            CHECK(pcm->GetPlayerController() == pc);
        }

        TEST_CASE("12. ViewTarget resolution") {
            auto world = UWorld::Create("ViewTargetWorld");
            auto pc = world->SpawnActor<APlayerController>("PC");
            world->AddPlayerController(pc);

            auto targetActor = world->SpawnActor<ACameraActor>("TargetCamera");
            targetActor->SetActorLocation({10.0f, 5.0f, 20.0f});
            targetActor->GetCameraComponent().Camera.SetPosition({10.0f, 5.0f, 20.0f});

            pc->SetViewTarget(targetActor);
            pc->UpdateCameraManager(0.016f);

            FPerspectiveCamera viewCam;
            pc->GetPlayerViewPoint(viewCam);
            CHECK(viewCam.GetPosition().x == doctest::Approx(10.0f));
            CHECK(viewCam.GetPosition().y == doctest::Approx(5.0f));
            CHECK(viewCam.GetPosition().z == doctest::Approx(20.0f));
        }

        TEST_CASE("12c. ViewTarget blend lerps position") {
            auto world = UWorld::Create("ViewBlendWorld");
            auto pc = world->SpawnActor<APlayerController>("PC");
            world->AddPlayerController(pc);

            auto camA = world->SpawnActor<ACameraActor>("CamA");
            camA->GetCameraComponent().Camera.SetPosition({0.0f, 1.0f, 0.0f});
            auto camB = world->SpawnActor<ACameraActor>("CamB");
            camB->GetCameraComponent().Camera.SetPosition({10.0f, 1.0f, 0.0f});

            pc->SetViewTarget(camA);
            pc->UpdateCameraManager(0.016f);
            pc->SetViewTargetWithBlend(camB, 1.0f);
            pc->UpdateCameraManager(0.5f);

            FPerspectiveCamera viewCam;
            pc->GetPlayerViewPoint(viewCam);
            CHECK(viewCam.GetPosition().x == doctest::Approx(5.0f).epsilon(0.05));
            REQUIRE(pc->GetPlayerCameraManager());
            CHECK(pc->GetPlayerCameraManager()->IsBlendingViewTarget());
        }

        TEST_CASE("12d. Spectator mode uses view target character") {
            auto world = UWorld::Create("SpectatorWorld");
            auto pc = world->SpawnActor<APlayerController>("PC");
            world->AddPlayerController(pc);

            auto localPawn = world->SpawnActor<ACharacter>("LocalPawn");
            localPawn->SetActorLocation({0.0f, 1.7f, 0.0f});
            auto targetPawn = world->SpawnActor<ACharacter>("TargetPawn");
            targetPawn->SetActorLocation({12.0f, 1.7f, 4.0f});
            targetPawn->SetControlPitch(-5.0f);
            targetPawn->SetControlYaw(45.0f);
            pc->Possess(localPawn);

            pc->EnterSpectatorMode(targetPawn);
            CHECK(pc->IsSpectating());
            CHECK(pc->GetViewTarget() == targetPawn);

            pc->UpdateCameraManager(0.016f);
            FPerspectiveCamera viewCam;
            pc->GetPlayerViewPoint(viewCam);
            CHECK(viewCam.GetPosition().x == doctest::Approx(12.0f).epsilon(0.5f));
            CHECK(viewCam.GetPitch() == doctest::Approx(-5.0f).epsilon(0.1f));
            CHECK(viewCam.GetYaw() == doctest::Approx(45.0f).epsilon(0.1f));

            pc->LeaveSpectatorMode();
            CHECK_FALSE(pc->IsSpectating());
            CHECK(pc->GetViewTarget() == localPawn);
        }

        TEST_CASE("12b. AProjectile movement") {
            auto world = UWorld::Create("ProjectileWorld");
            auto* proj = world->SpawnActor<AProjectile>("Rocket");
            REQUIRE(proj != nullptr);
            REQUIRE(proj->GetProjectileMovement() != nullptr);
            proj->SetActorLocation({0.0f, 2.0f, 0.0f});
            proj->GetProjectileMovement()->InitialSpeed = 10.0f;
            proj->GetProjectileMovement()->ProjectileGravityScale = 0.0f;
            proj->SetInitialLifeSpan(2.0f);
            proj->InitVelocity({0.0f, 0.0f, 1.0f});
            world->BeginPlay();
            world->Tick(FTimestep(0.05f));
            CHECK(proj->GetActorLocation().z == doctest::Approx(0.5f).epsilon(0.25f));
            CHECK_FALSE(proj->HasExploded());
        }

        TEST_CASE("13. Default camera fallback") {
            auto world = UWorld::Create("FallbackCamWorld");
            auto pc = world->SpawnActor<APlayerController>("PC");
            world->AddPlayerController(pc);

            pc->SetViewTarget(nullptr);
            pc->UpdateCameraManager(0.016f);

            FPerspectiveCamera fallbackCam;
            pc->GetPlayerViewPoint(fallbackCam);
            CHECK(fallbackCam.GetPosition().y == doctest::Approx(3.5f));
        }

        TEST_CASE("14. UWorld BeginPlay & 15. UWorld Tick") {
            auto world = UWorld::Create("LifecycleWorld");
            auto actor = world->SpawnActor<ATestLifecycleActor>("Actor");

            CHECK(!world->HasBegunPlay());
            CHECK(!actor->bBeginPlayCalled);

            world->BeginPlay();
            CHECK(world->HasBegunPlay());
            CHECK(actor->bBeginPlayCalled);

            world->Tick(FTimestep(0.0333f));
            CHECK(actor->bTickCalled);
            CHECK(actor->TotalDelta == doctest::Approx(0.0333f));

            world->EndPlay();
            CHECK(!world->HasBegunPlay());
            CHECK(actor->bEndPlayCalled);
        }

        TEST_CASE("16. .lmap serialization/deserialization") {
            auto srcWorld = UWorld::Create("SyntheticMap");

            auto actor1 = srcWorld->SpawnActor<AActor>("Sun");
            actor1->SetActorLocation({0.0f, 10.0f, 0.0f});
            actor1->AddComponent<FDirectionalLightComponent>();

            auto actor2 = srcWorld->SpawnActor<AActor>("Lamp");
            actor2->SetActorLocation({5.0f, 3.0f, 5.0f});
            actor2->AddComponent<FPointLightComponent>();

            FMapSerializer serializer(srcWorld);
            std::string serialized;
            REQUIRE(serializer.SerializeText(serialized));

            auto dstWorld = UWorld::Create("ReconstructedMap");
            FMapSerializer deserializer(dstWorld);
            REQUIRE(deserializer.DeserializeText(serialized));

            CHECK(dstWorld->GetAllActors().size() >= 2);
            auto restoredSun = dstWorld->FindActorByName("Sun");
            REQUIRE(restoredSun != nullptr);
            CHECK(restoredSun->GetActorLocation().y == doctest::Approx(10.0f));
            CHECK(restoredSun->HasComponent<FDirectionalLightComponent>());

            auto restoredLamp = dstWorld->FindActorByName("Lamp");
            REQUIRE(restoredLamp != nullptr);
            CHECK(restoredLamp->GetActorLocation().x == doctest::Approx(5.0f));
            CHECK(restoredLamp->HasComponent<FPointLightComponent>());
        }

        TEST_CASE("17. DefaultEngine.ini parsing & 18. GameMode configuration resolution") {
            std::string testIniContent = "[/Script/EngineSettings.GameMapsSettings]\n"
                                         "GameDefaultMap=Projects/Sandbox/Content/Maps/ShowcaseLevel.lmap\n"
                                         "GlobalDefaultGameMode=AGameModeBase\n\n"
                                         "[/Script/Engine.GameModeBase]\n"
                                         "DefaultPawnClass=ADefaultPawn\n"
                                         "PlayerControllerClass=APlayerController\n"
                                         "GameStateClass=AGameStateBase\n"
                                         "PlayerStateClass=APlayerState\n\n"
                                         "[/Script/Engine.DisplaySettings]\n"
                                         "WindowTitle=LeonEngine2 Test Suite\n"
                                         "WindowWidth=1920\n"
                                         "WindowHeight=1080\n"
                                         "VSync=True\n"
                                         "Fullscreen=False\n";

            std::string tempIniPath = "build/Test_DefaultEngine.ini";
            std::ofstream fout(tempIniPath);
            fout << testIniContent;
            fout.close();

            FConfigFile config;
            REQUIRE(config.Load(tempIniPath));

            CHECK(config.GetString("/Script/EngineSettings.GameMapsSettings", "GameDefaultMap", "") ==
                  "Projects/Sandbox/Content/Maps/ShowcaseLevel.lmap");
            CHECK(config.GetString("/Script/EngineSettings.GameMapsSettings", "GlobalDefaultGameMode", "") ==
                  "AGameModeBase");
            CHECK(config.GetString("/Script/Engine.GameModeBase", "DefaultPawnClass", "") == "ADefaultPawn");
            CHECK(config.GetString("/Script/Engine.GameModeBase", "PlayerControllerClass", "") == "APlayerController");
            CHECK(config.GetString("/Script/Engine.GameModeBase", "GameStateClass", "") == "AGameStateBase");
            CHECK(config.GetString("/Script/Engine.GameModeBase", "PlayerStateClass", "") == "APlayerState");
        }

        TEST_CASE("Comprehensive: Blank World Bootstrap produces complete gameplay framework & valid view") {
            // Scenario: Zero custom classes or configuration.
            // AGameModeBase must automatically spin up GameState, PlayerController,
            // PlayerState, ADefaultPawn, APlayerCameraManager and deliver a valid viewpoint.
            auto world = UWorld::Create("BlankBootstrapWorld");

            auto gameMode = world->SpawnActor<AGameModeBase>("GameMode");
            world->SetGameMode(gameMode);
            world->InitWorld();
            world->BeginPlay();

            // 1. GameState created
            AGameStateBase* gameState = world->GetGameState();
            REQUIRE(gameState != nullptr);

            // 2. PlayerController created
            APlayerController* pc = world->GetFirstPlayerController();
            REQUIRE(pc != nullptr);

            // 3. PlayerState created and registered in GameState
            APlayerState* ps = pc->GetPlayerState();
            REQUIRE(ps != nullptr);
            CHECK(gameState->GetPlayerArray().size() == 1);
            CHECK(gameState->GetPlayerArray()[0] == ps);

            // 4. DefaultPawn created and possessed
            APawn* pawn = pc->GetPawn();
            REQUIRE(pawn != nullptr);
            CHECK(pawn->GetController() == pc);
            CHECK(pawn->HasComponent<FCameraComponent>());

            // 5. PlayerCameraManager created
            APlayerCameraManager* camManager = pc->GetPlayerCameraManager();
            REQUIRE(camManager != nullptr);

            // 6. GetPlayerViewPoint yields a valid perspective camera with non-degenerate matrices
            FPerspectiveCamera viewCamera;
            pc->GetPlayerViewPoint(viewCamera);
            const glm::mat4& viewProj = viewCamera.GetViewProjectionMatrix();
            CHECK(viewProj[3][3] != 1.0f); // Standard perspective projection marker
            CHECK(viewCamera.GetPosition().y == doctest::Approx(3.5f));
        }

        TEST_CASE("19. FProjectDescriptor JSON serialization & roundtrip") {
            FProjectDescriptor desc;
            desc.FileVersion = 1;
            desc.EngineVersion = "0.15.0";
            desc.ProjectName = "TestGame";
            desc.DefaultMap = "/Game/Maps/TestMap";
            desc.DefaultGameMode = "AGameModeBase";

            std::string serialized = desc.SerializeJson();
            CHECK(serialized.find("\"ProjectName\": \"TestGame\"") != std::string::npos);
            CHECK(serialized.find("\"DefaultMap\": \"/Game/Maps/TestMap\"") != std::string::npos);

            FProjectDescriptor restored;
            REQUIRE(restored.DeserializeJson(serialized));
            CHECK(restored.FileVersion == 1);
            CHECK(restored.EngineVersion == "0.15.0");
            CHECK(restored.ProjectName == "TestGame");
            CHECK(restored.DefaultMap == "/Game/Maps/TestMap");
            CHECK(restored.DefaultGameMode == "AGameModeBase");
        }

        TEST_CASE("20. FProjectPaths virtual path resolution (/Game/..., /Engine/...)") {
            FProjectPaths::SetProjectRoot("Projects/TestProject/TestProject.lproject");
            CHECK(FProjectPaths::ProjectDir() == "Projects/TestProject");
            CHECK(FProjectPaths::ProjectContentDir() == "Projects/TestProject/Content");
            CHECK(FProjectPaths::ProjectConfigDir() == "Projects/TestProject/Config");

            // Test /Game/ resolution
            std::string resolvedGame = FProjectPaths::ResolveVirtualPath("/Game/Maps/TestMap.lmap");
            CHECK(resolvedGame == "Projects/TestProject/Content/Maps/TestMap.lmap");

            // Test /Engine/ resolution
            std::string resolvedEngine = FProjectPaths::ResolveVirtualPath("/Engine/Shaders/PBR_Lit.glsl");
            CHECK(resolvedEngine == "Engine/Resources/Shaders/PBR_Lit.glsl");

            // Test MakeVirtualPath
            std::string virtGame = FProjectPaths::MakeVirtualPath("Projects/TestProject/Content/Textures/T_Test.ltex");
            CHECK(virtGame == "/Game/Textures/T_Test.ltex");

            std::string remapped =
                FProjectPaths::ResolveVirtualPath("c:/old/Projects/StaleName/Content/Animations/Idle.lanim");
            CHECK(remapped == "Projects/TestProject/Content/Animations/Idle.lanim");
            std::string virtStale =
                FProjectPaths::MakeVirtualPath("c:/old/Projects/StaleName/Content/Animations/Idle.lanim");
            CHECK(virtStale == "/Game/Animations/Idle.lanim");
        }

        TEST_CASE("21. Multi-INI Configuration System (Engine, Game, FInput)") {
            std::string tempDir = "build/TestConfigs";
            std::filesystem::create_directories(tempDir);

            std::string engineIniPath = tempDir + "/DefaultEngine.ini";
            std::string gameIniPath = tempDir + "/DefaultGame.ini";
            std::string inputIniPath = tempDir + "/DefaultInput.ini";

            {
                std::ofstream f(engineIniPath);
                f << "[/Script/EngineSettings.GameMapsSettings]\n"
                  << "GameDefaultMap=/Game/Maps/MyMap\n"
                  << "GlobalDefaultGameMode=AGameModeBase\n\n"
                  << "[/Script/Engine.DisplaySettings]\n"
                  << "WindowTitle=Multi-Config Project\n"
                  << "WindowWidth=1920\n";
            }
            {
                std::ofstream f(gameIniPath);
                f << "[/Script/Engine.GameModeBase]\n"
                  << "DefaultPawnClass=ADefaultPawn\n"
                  << "PlayerControllerClass=APlayerController\n";
            }
            {
                std::ofstream f(inputIniPath);
                f << "[/Script/Engine.InputSettings]\n"
                  << "bEnableMouseLook=True\n"
                  << "MoveForwardKey=W\n";
            }

            FConfigFile engineConfig(engineIniPath);
            FConfigFile gameConfig(gameIniPath);
            FConfigFile inputConfig(inputIniPath);

            CHECK(engineConfig.GetString("/Script/EngineSettings.GameMapsSettings", "GameDefaultMap", "") ==
                  "/Game/Maps/MyMap");
            CHECK(engineConfig.GetInt("/Script/Engine.DisplaySettings", "WindowWidth", 0) == 1920);
            CHECK(gameConfig.GetString("/Script/Engine.GameModeBase", "DefaultPawnClass", "") == "ADefaultPawn");
            CHECK(inputConfig.GetBool("/Script/Engine.InputSettings", "bEnableMouseLook", false) == true);
            CHECK(inputConfig.GetString("/Script/Engine.InputSettings", "MoveForwardKey", "") == "W");
        }

        TEST_CASE("22. Single tick does not double-count GameState ElapsedTime") {
            auto world = UWorld::Create("ElapsedWorld");
            auto* gm = world->SpawnActor<AGameModeBase>("GM");
            world->SetGameMode(gm);
            world->InitWorld();
            world->BeginPlay();
            AGameStateBase* gs = world->GetGameState();
            REQUIRE(gs);
            world->Tick(FTimestep(0.1f));
            CHECK(gs->GetElapsedTime() == doctest::Approx(0.1f));
        }

        TEST_CASE("23. Destroy PlayerController then Tick is safe") {
            auto world = UWorld::Create("DestroyPCWorld");
            auto* gm = world->SpawnActor<AGameModeBase>("GM");
            world->SetGameMode(gm);
            world->InitWorld();
            world->BeginPlay();
            APlayerController* pc = world->GetFirstPlayerController();
            REQUIRE(pc);
            world->DestroyActor(pc);
            CHECK(world->GetFirstPlayerController() == nullptr);
            world->Tick(FTimestep(0.016f));
            CHECK(world->GetFirstPlayerController() == nullptr);
        }

        TEST_CASE("24. InitGame is idempotent") {
            auto world = UWorld::Create("InitOnceWorld");
            auto* gm = world->SpawnActor<AGameModeBase>("GM");
            world->SetGameMode(gm);
            world->InitWorld();
            AGameStateBase* gs1 = world->GetGameState();
            REQUIRE(gs1);
            world->InitWorld();
            CHECK(world->GetGameState() == gs1);
            size_t gsCount = 0;
            for (const auto& a : world->GetAllActors()) {
                if (dynamic_cast<AGameStateBase*>(a.get()))
                    ++gsCount;
            }
            CHECK(gsCount == 1);
        }

        TEST_CASE("25. Tick no-ops before BeginPlay") {
            auto world = UWorld::Create("NoPlayTick");
            auto* gs = world->SpawnActor<AGameStateBase>("GS");
            world->SetGameState(gs);
            world->Tick(FTimestep(0.25f));
            CHECK(gs->GetElapsedTime() == doctest::Approx(0.0f));
        }

        TEST_CASE("26. .lmap Class+GUID roundtrip and ACameraActor survives") {
            auto src = UWorld::Create("GuidMap");
            auto* sun = src->SpawnActor<AActor>("Sun");
            sun->SetClass("AActor");
            sun->AddComponent<FDirectionalLightComponent>();
            auto* cam = src->SpawnActor<ACameraActor>("CineCam");
            cam->SetActorLocation({1.0f, 2.0f, 3.0f});

            FMapSerializer serializer(src);
            std::string yaml;
            REQUIRE(serializer.SerializeText(yaml));
            CHECK(yaml.find("Class:") != std::string::npos);
            CHECK(yaml.find("GUID:") != std::string::npos);
            CHECK(yaml.find("ACameraActor") != std::string::npos);

            auto dst = UWorld::Create("GuidMapDst");
            FMapSerializer deserializer(dst);
            REQUIRE(deserializer.DeserializeText(yaml));
            auto* restoredCam = dynamic_cast<ACameraActor*>(dst->FindActorByName("CineCam"));
            REQUIRE(restoredCam);
            CHECK(restoredCam->GetActorLocation().y == doctest::Approx(2.0f));
            CHECK(restoredCam->GetClass() == "ACameraActor");
            auto* restoredSun = dst->FindActorByName("Sun");
            REQUIRE(restoredSun);
            CHECK(restoredSun->GetActorGuid() == sun->GetActorGuid());
        }

        TEST_CASE("27. Maps without Class/GUID still load") {
            const char* kOld = R"(
Map:
  Name: "Legacy"
Actors:
  - Name: "Prop"
    Transform:
      Translation: [4.0, 5.0, 6.0]
      Rotation: [0, 0, 0]
      Scale: [1, 1, 1]
)";
            auto world = UWorld::Create("LegacyLoad");
            FMapSerializer s(world);
            REQUIRE(s.DeserializeText(kOld));
            auto* prop = world->FindActorByName("Prop");
            REQUIRE(prop);
            CHECK(prop->GetActorLocation().x == doctest::Approx(4.0f));
            CHECK(prop->GetActorGuid().IsValid());
        }

        TEST_CASE("28. Login spawns pawn at APlayerStart") {
            auto world = UWorld::Create("StartWorld");
            auto* start = world->SpawnActor<APlayerStart>("PlayerStart");
            start->SetActorLocation({9.0f, 4.0f, 7.0f});
            auto* gm = world->SpawnActor<AGameModeBase>("GM");
            world->SetGameMode(gm);
            world->InitWorld();
            world->BeginPlay();
            APawn* pawn = world->GetFirstPlayerController()->GetPawn();
            REQUIRE(pawn);
            CHECK(pawn->GetActorLocation().x == doctest::Approx(9.0f));
            CHECK(pawn->GetActorLocation().z == doctest::Approx(7.0f));
        }

        TEST_CASE("28b. ChoosePlayerStart takes first enabled non-Dummy start") {
            auto world = UWorld::Create("ChooseStartWorld");
            auto* dummy = world->SpawnActor<APlayerStart>("DummyStart");
            dummy->SetPlayerStartTag("Dummy");
            dummy->SetTeamIndex(1);
            auto* first = world->SpawnActor<APlayerStart>("Team2Start");
            first->SetTeamIndex(2);
            first->SetActorLocation({1.0f, 0.0f, 0.0f});
            auto* preferredTeam = world->SpawnActor<APlayerStart>("Team1Start");
            preferredTeam->SetTeamIndex(1);
            preferredTeam->SetPlayerStartTag("Player");
            preferredTeam->SetActorLocation({9.0f, 0.0f, 0.0f});

            auto* gm = world->SpawnActor<AGameModeBase>("GM");
            CHECK(gm->ChoosePlayerStart() == first);
            CHECK(gm->ChoosePlayerStart(nullptr) == first);
            CHECK_FALSE(first->CanEverTick());
        }

        TEST_CASE("29. Character AABB blocked by static cube") {
            auto world = UWorld::Create("ColWorld");
            auto* cube = world->SpawnActor<AActor>("Cube");
            cube->SetActorLocation({2.0f, 0.5f, 0.0f});
            auto& mesh = cube->AddComponent<FMeshComponent>();
            mesh.MeshType = "Cube";
            mesh.MeshSize = 1.0f;
            mesh.Mobility = EComponentMobility::Static;

            auto* character = world->SpawnActor<ACharacter>("Char");
            character->SetFloorZ(0.0f);
            character->SetActorLocation({0.0f, 1.7f, 0.0f});

            UWorld::FHitResult overlap;
            glm::vec3 qmin(-0.4f, 0.0f, -0.4f);
            glm::vec3 qmax(0.4f, 1.9f, 0.4f);
            glm::vec3 cubeMin(1.5f, 0.0f, -0.5f);
            glm::vec3 cubeMax(2.5f, 1.0f, 0.5f);
            CHECK(world->OverlapAABB(cubeMin, cubeMax, character, overlap));
            CHECK(overlap.Actor == cube);

            character->MoveBlocked({2.0f, 0.0f, 0.0f});
            CHECK(character->GetActorLocation().x < 1.5f);
        }

        TEST_CASE("30. UEngine travel is transactional") {
            UEngine engine;
            auto gi = std::make_shared<UGameInstance>("GI");
            auto world = UWorld::Create("KeepWorld");
            auto* gm = world->SpawnActor<AGameModeBase>("GM");
            world->SetGameMode(gm);
            world->InitWorld();
            world->BeginPlay();
            REQUIRE(world->GetFirstPlayerController());
            engine.BindSession(gi, world);

            CHECK_FALSE(engine.TravelToMap("/Game/Maps/DoesNotExist_ZZZ"));
            CHECK(engine.GetWorld().get() == world.get());
            CHECK(engine.GetWorld()->GetFirstPlayerController() != nullptr);

            std::filesystem::create_directories("build/TravelMaps");
            const std::string mapPath = "build/TravelMaps/TravelOk.lmap";
            {
                std::ofstream f(mapPath);
                f << "Map:\n  Name: \"TravelOk\"\nActors:\n  - Name: \"Marker\"\n    Transform:\n"
                  << "      Translation: [0, 0, 0]\n      Rotation: [0, 0, 0]\n      Scale: [1, 1, 1]\n";
            }
            FProjectPaths::SetProjectRoot("build/TravelMaps/Dummy.lproject");
            // Physical path used directly: ResolveVirtualPath may not map this. Write using a /Game path after
            // pointing content root.
            std::filesystem::create_directories("build/TravelProject/Content/Maps");
            const std::string virtMap = "build/TravelProject/Content/Maps/Ok.lmap";
            {
                std::ofstream f(virtMap);
                f << "Map:\n  Name: \"Ok\"\nActors:\n  - Name: \"Marker\"\n    Transform:\n"
                  << "      Translation: [1, 2, 3]\n      Rotation: [0, 0, 0]\n      Scale: [1, 1, 1]\n";
            }
            FProjectPaths::SetProjectRoot("build/TravelProject/Dummy.lproject");
            REQUIRE(engine.TravelToMap("/Game/Maps/Ok"));
            REQUIRE(engine.GetWorld());
            CHECK(engine.GetWorld()->FindActorByName("Marker") != nullptr);
            REQUIRE(engine.GetWorld()->GetGameMode());
            REQUIRE(engine.GetWorld()->GetFirstPlayerController());
            REQUIRE(engine.GetWorld()->GetFirstPlayerController()->GetPawn());
        }

        TEST_CASE("31. UnloadUnused drops unused cache entries") {
            auto mesh = UStaticMesh::Create("TempUnused");
            UAssetManager::AddStaticMesh("TempUnusedKey", mesh);
            CHECK(UAssetManager::HasStaticMesh("TempUnusedKey"));
            mesh.reset();
            UAssetManager::UnloadUnused();
            CHECK_FALSE(UAssetManager::HasStaticMesh("TempUnusedKey"));
        }

        TEST_CASE("32. material instances are unique per request") {
            auto parent = UAssetManager::GetDefaultMaterial();
            REQUIRE(parent);
            auto a = parent->CreateInstance();
            auto b = parent->CreateInstance();
            REQUIRE(a);
            REQUIRE(b);
            CHECK(a.get() != b.get());
            CHECK(a->GetParent().get() == parent.get());
            CHECK(b->GetParent().get() == parent.get());
        }

        TEST_CASE("33. default material instance is cached") {
            auto a = UAssetManager::GetDefaultMaterialInstance();
            auto b = UAssetManager::GetDefaultMaterialInstance();
            REQUIRE(a);
            REQUIRE(b);
            CHECK(a.get() == b.get());
        }

    } // TEST_SUITE

} // namespace Leon
