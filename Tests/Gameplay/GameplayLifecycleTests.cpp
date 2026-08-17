#include <doctest/doctest.h>

#include "Core/FProjectPaths.hpp"
#include "Core/FTimestep.hpp"
#include "Engine/UEngine.hpp"
#include "Engine/UGameInstance.hpp"
#include "Engine/UWorld.hpp"
#include "Gameplay/AGameModeBase.hpp"
#include "Gameplay/AGameStateBase.hpp"
#include "Gameplay/APawn.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Gameplay/APlayerState.hpp"
#include "Gameplay/UHealthComponent.hpp"
#include "Gameplay/AActor.hpp"
#include "Assets/FAssetTypes.hpp"
#include "Engine/ENetTypes.hpp"

#include <filesystem>
#include <fstream>

namespace Leon {

    namespace {
        size_t CountLiveActors(UWorld* InWorld) {
            size_t n = 0;
            if (!InWorld)
                return 0;
            for (const auto& actor : InWorld->GetAllActors()) {
                if (actor && !actor->IsPendingKill())
                    ++n;
            }
            return n;
        }

        void WriteTravelMap(const std::string& InPath, const std::string& InName) {
            std::filesystem::create_directories(std::filesystem::path(InPath).parent_path());
            std::ofstream f(InPath);
            f << "Map:\n  Name: \"" << InName << "\"\nActors:\n  - Name: \"Marker\"\n    Transform:\n"
              << "      Translation: [1, 2, 3]\n      Rotation: [0, 0, 0]\n      Scale: [1, 1, 1]\n";
        }
    } // namespace

    TEST_SUITE("Gameplay framework lifecycle") {

        TEST_CASE("Login spawns controller PlayerState and pawn then possesses") {
            auto world = UWorld::Create("LoginLife");
            auto* gm = world->SpawnActor<AGameModeBase>("GM");
            world->SetGameMode(gm);
            world->InitWorld();
            world->BeginPlay();

            APlayerController* pc = world->GetFirstPlayerController();
            REQUIRE(pc);
            APlayerState* ps = pc->GetPlayerState();
            REQUIRE(ps);
            APawn* pawn = pc->GetPawn();
            REQUIRE(pawn);
            CHECK(pawn->GetController() == pc);
            CHECK(pawn->GetPlayerState() == ps);
            CHECK(world->GetGameState());
            CHECK(world->GetGameState()->GetPlayerArray().size() == 1);
        }

        TEST_CASE("RestartPlayer destroys pawn and keeps controller plus PlayerState") {
            auto world = UWorld::Create("RestartLife");
            auto* gm = world->SpawnActor<AGameModeBase>("GM");
            world->SetGameMode(gm);
            world->InitWorld();
            world->BeginPlay();

            APlayerController* pc = world->GetFirstPlayerController();
            REQUIRE(pc);
            APlayerState* ps = pc->GetPlayerState();
            REQUIRE(ps);
            APawn* oldPawn = pc->GetPawn();
            REQUIRE(oldPawn);
            const FUUID oldGuid = oldPawn->GetActorGuid();
            ps->SetPlayerName("KeepMe");

            gm->RestartPlayer(pc);
            APawn* spawned = pc->GetPawn();
            REQUIRE(spawned);
            CHECK(spawned->GetActorGuid() != oldGuid);
            CHECK(world->FindActorByGuid(oldGuid) == nullptr);
            CHECK(pc->GetPlayerState() == ps);
            CHECK(ps->GetPlayerName() == "KeepMe");
            CHECK(spawned->GetPlayerState() == ps);
            CHECK(world->GetFirstPlayerController() == pc);
        }

        TEST_CASE("StartPlay on a client world does not Login") {
            auto world = UWorld::Create("ClientLife");
            world->SetNetMode(ENetMode::Client);
            auto* gm = world->SpawnActor<AGameModeBase>("GM");
            world->SetGameMode(gm);
            world->InitWorld();
            world->BeginPlay();
            CHECK(world->GetPlayerControllers().empty());
            CHECK(world->GetFirstPlayerController() == nullptr);
            REQUIRE(world->GetGameState());
        }

        TEST_CASE("two Logins create two PlayerStates") {
            auto world = UWorld::Create("TwoLogin");
            auto* gm = world->SpawnActor<AGameModeBase>("GM");
            world->SetGameMode(gm);
            world->InitWorld();
            world->BeginPlay();
            REQUIRE(gm->Login("Player_1"));
            CHECK(world->GetPlayerControllers().size() == 2);
            REQUIRE(world->GetGameState());
            CHECK(world->GetGameState()->GetPlayerArray().size() == 2);
            CHECK(world->GetPlayerControllers()[0]->GetPawn());
            CHECK(world->GetPlayerControllers()[1]->GetPawn());
        }

        TEST_CASE("GameInstance survives travel") {
            UEngine engine;
            auto gi = std::make_shared<UGameInstance>("PersistGI");
            auto world = UWorld::Create("TravelSrc");
            auto* gm = world->SpawnActor<AGameModeBase>("GM");
            world->SetGameMode(gm);
            world->InitWorld();
            world->BeginPlay();
            engine.BindSession(gi, world);
            UGameInstance* giPtr = engine.GetGameInstance().get();

            std::filesystem::create_directories("build/TravelProject/Content/Maps");
            WriteTravelMap("build/TravelProject/Content/Maps/LifeA.lmap", "LifeA");
            FProjectPaths::SetProjectRoot("build/TravelProject/Dummy.lproject");
            REQUIRE(engine.TravelToMap("/Game/Maps/LifeA"));
            CHECK(engine.GetGameInstance().get() == giPtr);
            CHECK(engine.GetWorld().get() != world.get());
            REQUIRE(engine.GetWorld()->GetGameMode());
            REQUIRE(engine.GetWorld()->GetFirstPlayerController());
            REQUIRE(engine.GetWorld()->GetFirstPlayerController()->GetPawn());
        }

        TEST_CASE("repeated travel does not grow live actors") {
            UEngine engine;
            auto gi = std::make_shared<UGameInstance>("CycleGI");
            auto world = UWorld::Create("CycleSrc");
            auto* gm = world->SpawnActor<AGameModeBase>("GM");
            world->SetGameMode(gm);
            world->InitWorld();
            world->BeginPlay();
            engine.BindSession(gi, world);

            std::filesystem::create_directories("build/TravelProject/Content/Maps");
            WriteTravelMap("build/TravelProject/Content/Maps/LifeB.lmap", "LifeB");
            WriteTravelMap("build/TravelProject/Content/Maps/LifeC.lmap", "LifeC");
            FProjectPaths::SetProjectRoot("build/TravelProject/Dummy.lproject");
            REQUIRE(engine.TravelToMap("/Game/Maps/LifeB"));
            const size_t afterFirst = CountLiveActors(engine.GetWorld().get());
            REQUIRE(engine.TravelToMap("/Game/Maps/LifeC"));
            const size_t afterSecond = CountLiveActors(engine.GetWorld().get());
            REQUIRE(engine.TravelToMap("/Game/Maps/LifeB"));
            const size_t afterThird = CountLiveActors(engine.GetWorld().get());
            CHECK(afterSecond == afterFirst);
            CHECK(afterThird == afterFirst);
            CHECK(afterFirst >= 4);
        }
    }

} // namespace Leon
