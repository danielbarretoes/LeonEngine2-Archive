#include <doctest/doctest.h>

#include "Core/FTimestep.hpp"
#include "Engine/ULoopbackNetDriver.hpp"
#include "Engine/UIpNetDriver.hpp"
#include "Engine/UGameInstance.hpp"
#include "Engine/INetTransport.hpp"
#include "Engine/UWorld.hpp"
#include "Gameplay/AGameModeBase.hpp"
#include "Gameplay/AGameStateBase.hpp"
#include "Gameplay/APawn.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Gameplay/APlayerState.hpp"
#include "Gameplay/ACharacter.hpp"

#include <memory>

namespace Leon {

    TEST_SUITE("Listen-server loopback") {

        TEST_CASE("Server Login twice replicates PlayerStates and pawn transforms") {
            auto serverWorld = UWorld::Create("Server");
            auto clientWorld = UWorld::Create("Client");
            serverWorld->SetNetMode(ENetMode::ListenServer);
            clientWorld->SetNetMode(ENetMode::Client);

            ULoopbackNetDriver serverDriver;
            ULoopbackNetDriver clientDriver;
            serverDriver.SetWorld(serverWorld.get());
            clientDriver.SetWorld(clientWorld.get());
            ULoopbackNetDriver::Pair(serverDriver, clientDriver);
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

        TEST_CASE("Client ServerRPC mutates authority actor state over loopback") {
            auto serverWorld = UWorld::Create("ServerRpc");
            auto clientWorld = UWorld::Create("ClientRpc");
            serverWorld->SetNetMode(ENetMode::ListenServer);
            clientWorld->SetNetMode(ENetMode::Client);

            ULoopbackNetDriver serverDriver;
            ULoopbackNetDriver clientDriver;
            serverDriver.SetWorld(serverWorld.get());
            clientDriver.SetWorld(clientWorld.get());
            ULoopbackNetDriver::Pair(serverDriver, clientDriver);
            serverWorld->SetNetDriver(&serverDriver);
            clientWorld->SetNetDriver(&clientDriver);

            class ARpcProbePawn : public APawn {
            public:
                using APawn::APawn;
                int ServerCalls = 0;
                int ClientCalls = 0;
                bool HandleServerRPC(uint16_t InFunctionId, const uint8_t* InData, size_t InSize) override {
                    (void)InData;
                    (void)InSize;
                    if (InFunctionId == 7) {
                        ++ServerCalls;
                        return true;
                    }
                    return false;
                }
                bool HandleClientRPC(uint16_t InFunctionId, const uint8_t* InData, size_t InSize) override {
                    (void)InData;
                    (void)InSize;
                    if (InFunctionId == 9) {
                        ++ClientCalls;
                        return true;
                    }
                    return false;
                }
            };

            serverWorld->InitWorld();
            serverWorld->BeginPlay();
            clientWorld->InitWorld();
            clientWorld->BeginPlay();

            const FUUID guid = FUUID::Generate();
            auto* serverProbe = serverWorld->SpawnActor<ARpcProbePawn>("RpcProbeServer");
            auto* clientProbe = clientWorld->SpawnActor<ARpcProbePawn>("RpcProbeClient");
            REQUIRE(serverProbe);
            REQUIRE(clientProbe);
            serverProbe->SetActorGuid(guid);
            clientProbe->SetActorGuid(guid);
            serverProbe->SetLocalRole(ENetRole::Authority);
            clientProbe->SetLocalRole(ENetRole::AutonomousProxy);

            clientProbe->CallServerRPC(7);
            // Client Tick flushes OutgoingRPC via loopback; server Tick consumes IncomingRPC.
            clientWorld->Tick(FTimestep(0.05f));
            serverWorld->Tick(FTimestep(0.05f));
            CHECK(serverProbe->ServerCalls == 1);

            serverProbe->CallMulticastRPC(9);
            serverWorld->Tick(FTimestep(0.05f));
            clientWorld->Tick(FTimestep(0.05f));
            CHECK(serverProbe->ClientCalls == 1);
            CHECK(clientProbe->ClientCalls == 1);
        }

        TEST_CASE("Replicating actor spawns and destroys on client via snapshot") {
            auto serverWorld = UWorld::Create("ServerSpawn");
            auto clientWorld = UWorld::Create("ClientSpawn");
            serverWorld->SetNetMode(ENetMode::ListenServer);
            clientWorld->SetNetMode(ENetMode::Client);

            ULoopbackNetDriver serverDriver;
            ULoopbackNetDriver clientDriver;
            serverDriver.SetWorld(serverWorld.get());
            clientDriver.SetWorld(clientWorld.get());
            ULoopbackNetDriver::Pair(serverDriver, clientDriver);
            serverWorld->SetNetDriver(&serverDriver);
            clientWorld->SetNetDriver(&clientDriver);

            auto* gm = serverWorld->SpawnActor<AGameModeBase>("GameMode");
            serverWorld->SetGameMode(gm);
            serverWorld->InitWorld();
            serverWorld->BeginPlay();
            REQUIRE(serverWorld->GetGameMode()->Login("Host"));

            clientWorld->InitWorld();
            clientWorld->BeginPlay();

            auto* proj = serverWorld->SpawnActor<AActor>("RepProp");
            REQUIRE(proj);
            proj->SetClass("AActor");
            proj->SetReplicates(true);
            proj->SetAlwaysRelevant(true);
            proj->SetActorLocation({9.0f, 1.0f, 2.0f});
            const FUUID guid = proj->GetActorGuid();

            for (int i = 0; i < 3; ++i) {
                serverWorld->Tick(FTimestep(0.05f));
                clientWorld->Tick(FTimestep(0.05f));
            }

            AActor* clientActor = clientWorld->FindActorByGuid(guid);
            REQUIRE(clientActor);
            CHECK(clientActor->GetActorLocation().x == doctest::Approx(9.0f));
            CHECK(clientActor->GetLocalRole() == ENetRole::SimulatedProxy);

            serverWorld->DestroyActor(proj);
            for (int i = 0; i < 3; ++i) {
                serverWorld->Tick(FTimestep(0.05f));
                clientWorld->Tick(FTimestep(0.05f));
            }
            CHECK(clientWorld->FindActorByGuid(guid) == nullptr);
        }

        TEST_CASE("NetCullDistanceSquared drops far replicating actors") {
            auto serverWorld = UWorld::Create("ServerCull");
            auto clientWorld = UWorld::Create("ClientCull");
            serverWorld->SetNetMode(ENetMode::ListenServer);
            clientWorld->SetNetMode(ENetMode::Client);

            ULoopbackNetDriver serverDriver;
            ULoopbackNetDriver clientDriver;
            serverDriver.SetWorld(serverWorld.get());
            clientDriver.SetWorld(clientWorld.get());
            ULoopbackNetDriver::Pair(serverDriver, clientDriver);
            serverWorld->SetNetDriver(&serverDriver);
            clientWorld->SetNetDriver(&clientDriver);

            auto* gm = serverWorld->SpawnActor<AGameModeBase>("GameMode");
            serverWorld->SetGameMode(gm);
            serverWorld->InitWorld();
            serverWorld->BeginPlay();
            REQUIRE(serverWorld->GetGameMode()->Login("Host"));
            APawn* hostPawn = serverWorld->GetPlayerControllers()[0]->GetPawn();
            REQUIRE(hostPawn);
            hostPawn->SetActorLocation({0.0f, 0.0f, 0.0f});

            clientWorld->InitWorld();
            clientWorld->BeginPlay();

            auto* nearActor = serverWorld->SpawnActor<AActor>("Near");
            auto* farActor = serverWorld->SpawnActor<AActor>("Far");
            REQUIRE(nearActor);
            REQUIRE(farActor);
            nearActor->SetReplicates(true);
            farActor->SetReplicates(true);
            nearActor->SetAlwaysRelevant(false);
            farActor->SetAlwaysRelevant(false);
            nearActor->SetNetCullDistanceSquared(100.0f); // 10 units
            farActor->SetNetCullDistanceSquared(100.0f);
            nearActor->SetActorLocation({3.0f, 0.0f, 0.0f});
            farActor->SetActorLocation({50.0f, 0.0f, 0.0f});
            const FUUID nearGuid = nearActor->GetActorGuid();
            const FUUID farGuid = farActor->GetActorGuid();

            for (int i = 0; i < 3; ++i) {
                serverWorld->Tick(FTimestep(0.05f));
                clientWorld->Tick(FTimestep(0.05f));
            }

            CHECK(clientWorld->FindActorByGuid(nearGuid) != nullptr);
            CHECK(clientWorld->FindActorByGuid(farGuid) == nullptr);
        }

        TEST_CASE("AutonomousProxy keeps predicted location when snapshot arrives") {
            auto serverWorld = UWorld::Create("ServerPred");
            auto clientWorld = UWorld::Create("ClientPred");
            serverWorld->SetNetMode(ENetMode::ListenServer);
            clientWorld->SetNetMode(ENetMode::Client);

            ULoopbackNetDriver serverDriver;
            ULoopbackNetDriver clientDriver;
            serverDriver.SetWorld(serverWorld.get());
            clientDriver.SetWorld(clientWorld.get());
            ULoopbackNetDriver::Pair(serverDriver, clientDriver);
            serverWorld->SetNetDriver(&serverDriver);
            clientWorld->SetNetDriver(&clientDriver);

            auto* gm = serverWorld->SpawnActor<AGameModeBase>("GameMode");
            gm->DefaultPawnClass = "ACharacter";
            serverWorld->SetGameMode(gm);
            serverWorld->InitWorld();
            serverWorld->BeginPlay();
            REQUIRE(serverWorld->GetGameMode()->Login("Host"));
            REQUIRE(serverWorld->GetGameMode()->Login("Client"));

            clientWorld->InitWorld();
            clientWorld->BeginPlay();

            APlayerState* clientPs = serverWorld->GetPlayerControllers()[1]->GetPlayerState();
            REQUIRE(clientPs);
            clientDriver.SetLocalPlayerId(clientPs->GetPlayerId());

            APawn* serverClientPawn = serverWorld->GetPlayerControllers()[1]->GetPawn();
            REQUIRE(serverClientPawn);
            REQUIRE(dynamic_cast<ACharacter*>(serverClientPawn));
            serverClientPawn->SetActorLocation({0.0f, 0.0f, 0.0f});

            for (int i = 0; i < 3; ++i) {
                serverWorld->Tick(FTimestep(0.05f));
                clientWorld->Tick(FTimestep(0.05f));
            }

            AActor* clientPawn = clientWorld->FindActorByGuid(serverClientPawn->GetActorGuid());
            REQUIRE(clientPawn);
            CHECK(clientPawn->GetLocalRole() == ENetRole::AutonomousProxy);

            clientPawn->SetActorLocation({5.0f, 0.0f, 0.0f});
            serverClientPawn->SetActorLocation({0.0f, 0.0f, 0.0f});

            for (int i = 0; i < 2; ++i) {
                serverWorld->Tick(FTimestep(0.05f));
                clientWorld->Tick(FTimestep(0.05f));
            }

            CHECK(clientPawn->GetActorLocation().x == doctest::Approx(5.0f).epsilon(0.01f));
        }

        TEST_CASE("ACharacter pose history rewinds and restores on authority") {
            auto world = UWorld::Create("PoseHistory");
            world->SetNetMode(ENetMode::ListenServer);
            world->InitWorld();
            world->BeginPlay();

            auto* gs = world->SpawnActor<AGameStateBase>("GameState");
            world->SetGameState(gs);

            auto* character = world->SpawnActor<ACharacter>("PoseChar");
            REQUIRE(character);
            character->SetLocalRole(ENetRole::Authority);
            character->SetActorLocation({0.0f, 0.0f, 0.0f});

            for (int i = 0; i < 5; ++i) {
                character->SetActorLocation({static_cast<float>(i), 0.0f, 0.0f});
                world->Tick(FTimestep(0.05f));
            }

            const float rewindTime = gs->GetElapsedTime() - 0.1f;
            const glm::vec3 before = character->GetActorLocation();
            REQUIRE(character->RewindToTime(rewindTime));
            CHECK(character->GetActorLocation().x < before.x);
            character->RestoreNetPoseAfterRewind();
            CHECK(character->GetActorLocation().x == doctest::Approx(before.x));
        }
    }

    TEST_SUITE("UGameInstance session and FTimerManager") {

        class FTestNetTransport final : public INetTransport {
        public:
            bool Listen(uint16_t) override {
                bOpen = true;
                bServer = true;
                return true;
            }
            bool Connect(const std::string&, uint16_t) override {
                bOpen = true;
                bServer = false;
                return true;
            }
            void Close() override { bOpen = false; }
            void Poll() override {}
            bool Send(int32_t, const uint8_t*, size_t, bool) override { return true; }
            bool SendToAll(const uint8_t*, size_t, bool) override { return true; }
            int32_t ConsumeAcceptedConnection() override { return -1; }
            std::vector<FIncomingNetPacket> TakeIncoming() override { return {}; }
            int32_t GetConnectionCount() const override { return 0; }
            bool IsServer() const override { return bServer; }
            bool IsOpen() const override { return bOpen; }

        private:
            bool bOpen = false;
            bool bServer = false;
        };

        TEST_CASE("StartListenServer and ConnectToHost attach a driver") {
            UIpNetDriver::SetTransportFactory([]() { return std::make_unique<FTestNetTransport>(); });
            auto gi = MakeRef<UGameInstance>("GI");
            auto world = UWorld::Create("ListenWorld");
            CHECK(gi->StartListenServer(world.get(), 7777));
            CHECK(world->GetNetMode() == ENetMode::ListenServer);
            CHECK(world->GetNetDriver() != nullptr);
            gi->ShutdownNetDriver();
            CHECK(world->GetNetMode() == ENetMode::Standalone);

            auto client = UWorld::Create("JoinWorld");
            CHECK(gi->ConnectToHost(client.get(), "127.0.0.1", 7777));
            CHECK(client->GetNetMode() == ENetMode::Client);
            gi->ShutdownNetDriver();
            UIpNetDriver::SetTransportFactory({});
        }

        TEST_CASE("FTimerManager one-shot loop and clear") {
            auto world = UWorld::Create("TimerWorld");
            int fires = 0;
            FTimerHandle handle;
            world->GetTimerManager().SetTimer(handle, [&]() { ++fires; }, 0.5f, false);
            world->Tick(FTimestep(0.4f));
            CHECK(fires == 0);
            world->Tick(FTimestep(0.2f));
            CHECK(fires == 1);
            CHECK_FALSE(world->GetTimerManager().IsTimerActive(handle));

            FTimerHandle loop;
            world->GetTimerManager().SetTimer(loop, [&]() { ++fires; }, 0.25f, true);
            world->Tick(FTimestep(0.3f));
            world->Tick(FTimestep(0.3f));
            CHECK(fires >= 3);
            world->GetTimerManager().ClearTimer(loop);
            const int afterClear = fires;
            world->Tick(FTimestep(0.5f));
            CHECK(fires == afterClear);
        }
    }

} // namespace Leon
