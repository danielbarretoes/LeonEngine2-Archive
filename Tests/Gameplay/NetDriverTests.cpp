#include <doctest/doctest.h>

#include "Core/FTimestep.hpp"
#include "Engine/FLoopbackNetDriver.hpp"
#include "Engine/UWorld.hpp"
#include "Gameplay/AGameModeBase.hpp"
#include "Gameplay/AGameStateBase.hpp"
#include "Gameplay/APawn.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Gameplay/APlayerState.hpp"

namespace Leon {

    TEST_SUITE("Listen-server loopback") {

        TEST_CASE("Server Login twice replicates PlayerStates and pawn transforms") {
            auto serverWorld = UWorld::Create("Server");
            auto clientWorld = UWorld::Create("Client");
            serverWorld->SetNetMode(ENetMode::ListenServer);
            clientWorld->SetNetMode(ENetMode::Client);

            FLoopbackNetDriver serverDriver;
            FLoopbackNetDriver clientDriver;
            serverDriver.SetWorld(serverWorld.get());
            clientDriver.SetWorld(clientWorld.get());
            FLoopbackNetDriver::Pair(serverDriver, clientDriver);
            serverWorld->SetNetDriver(&serverDriver);
            clientWorld->SetNetDriver(&clientDriver);

            auto* gm = serverWorld->SpawnActor<AGameModeBase>("GameMode");
            serverWorld->SetGameMode(gm);
            serverWorld->InitWorld();
            serverWorld->BeginPlay();
            REQUIRE(serverWorld->GetGameMode()->Login("Player_1"));

            APawn* p0 = serverWorld->GetPlayerControllers()[0]->GetPawn();
            APawn* p1 = serverWorld->GetPlayerControllers()[1]->GetPawn();
            REQUIRE(p0);
            REQUIRE(p1);
            p0->SetActorLocation({1.0f, 2.0f, 3.0f});
            p1->SetActorLocation({4.0f, 5.0f, 6.0f});

            for (int i = 0; i < 4; ++i) {
                serverWorld->Tick(FTimestep(0.05f));
                clientWorld->Tick(FTimestep(0.05f));
            }

            AGameStateBase* clientGS = clientWorld->GetGameState();
            REQUIRE(clientGS);
            CHECK(clientGS->GetPlayerArray().size() == 2);
            CHECK(clientWorld->GetGameMode() == nullptr);

            AActor* c0 = clientWorld->FindActorByGuid(p0->GetActorGuid());
            AActor* c1 = clientWorld->FindActorByGuid(p1->GetActorGuid());
            REQUIRE(c0);
            REQUIRE(c1);
            CHECK(c0->GetActorLocation().x == doctest::Approx(1.0f));
            CHECK(c1->GetActorLocation().z == doctest::Approx(6.0f));
            CHECK(clientGS->GetElapsedTime() == doctest::Approx(serverWorld->GetGameState()->GetElapsedTime()));
        }

        TEST_CASE("connection identity is BoundPlayerId not controller index") {
            UNetConnection conn;
            CHECK(conn.BoundPlayerId == -1);
            conn.BoundPlayerId = 7;
            CHECK(conn.BoundPlayerId == 7);
        }
    }

} // namespace Leon
