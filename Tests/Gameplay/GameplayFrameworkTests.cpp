#include <doctest/doctest.h>
#include "Core/FConfigFile.hpp"
#include "Core/FTimestep.hpp"
#include "Gameplay/AActor.hpp"
#include "Gameplay/ACameraActor.hpp"
#include "Gameplay/ADefaultPawn.hpp"
#include "Gameplay/AGameModeBase.hpp"
#include "Gameplay/AGameStateBase.hpp"
#include "Gameplay/APawn.hpp"
#include "Gameplay/APlayerCameraManager.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Gameplay/APlayerState.hpp"
#include "Gameplay/UClassRegistry.hpp"
#include "Gameplay/UObject.hpp"
#include "Engine/Components.hpp"
#include "Engine/FMapSerializer.hpp"
#include "Engine/UGameInstance.hpp"
#include "Engine/UWorld.hpp"
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
            CHECK(registry.HasClass("AGameModeBase"));
            CHECK(registry.HasClass("ACameraActor"));
            CHECK(registry.HasClass("APlayerCameraManager"));
            CHECK(registry.HasClass("AHUD"));

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
            actor1->AddComponent<UDirectionalLightComponent>();

            auto actor2 = srcWorld->SpawnActor<AActor>("Lamp");
            actor2->SetActorLocation({5.0f, 3.0f, 5.0f});
            actor2->AddComponent<UPointLightComponent>();

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
            CHECK(restoredSun->HasComponent<UDirectionalLightComponent>());

            auto restoredLamp = dstWorld->FindActorByName("Lamp");
            REQUIRE(restoredLamp != nullptr);
            CHECK(restoredLamp->GetActorLocation().x == doctest::Approx(5.0f));
            CHECK(restoredLamp->HasComponent<UPointLightComponent>());
        }

        TEST_CASE("17. DefaultEngine.ini parsing & 18. GameMode configuration resolution") {
            std::string testIniContent =
                "[/Script/EngineSettings.GameMapsSettings]\n"
                "GameDefaultMap=Projects/Sandbox/Content/Maps/MainShowcase.lmap\n"
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
                  "Projects/Sandbox/Content/Maps/MainShowcase.lmap");
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
            CHECK(pawn->HasComponent<UCameraComponent>());

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
            desc.EngineVersion = "0.8.0";
            desc.ProjectName = "TestGame";
            desc.DefaultMap = "/Game/Maps/TestMap";
            desc.DefaultGameMode = "AGameModeBase";

            std::string serialized = desc.SerializeJson();
            CHECK(serialized.find("\"ProjectName\": \"TestGame\"") != std::string::npos);
            CHECK(serialized.find("\"DefaultMap\": \"/Game/Maps/TestMap\"") != std::string::npos);

            FProjectDescriptor restored;
            REQUIRE(restored.DeserializeJson(serialized));
            CHECK(restored.FileVersion == 1);
            CHECK(restored.EngineVersion == "0.8.0");
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
            CHECK(resolvedEngine == "Engine/Assets/Shaders/PBR_Lit.glsl");

            // Test MakeVirtualPath
            std::string virtGame = FProjectPaths::MakeVirtualPath("Projects/TestProject/Content/Textures/T_Test.ltex");
            CHECK(virtGame == "/Game/Textures/T_Test.ltex");
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

            CHECK(engineConfig.GetString("/Script/EngineSettings.GameMapsSettings", "GameDefaultMap", "") == "/Game/Maps/MyMap");
            CHECK(engineConfig.GetInt("/Script/Engine.DisplaySettings", "WindowWidth", 0) == 1920);
            CHECK(gameConfig.GetString("/Script/Engine.GameModeBase", "DefaultPawnClass", "") == "ADefaultPawn");
            CHECK(inputConfig.GetBool("/Script/Engine.InputSettings", "bEnableMouseLook", false) == true);
            CHECK(inputConfig.GetString("/Script/Engine.InputSettings", "MoveForwardKey", "") == "W");
        }

    } // TEST_SUITE

} // namespace Leon
