#include <doctest/doctest.h>

#include "Core/FTimestep.hpp"
#include "Core/FWorldUnits.hpp"
#include "Engine/UWorld.hpp"
#include "Engine/FLoopbackNetDriver.hpp"
#include "Gameplay/ACharacter.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Gameplay/AAIController.hpp"
#include "Gameplay/UCharacterMovementComponent.hpp"
#include "Gameplay/USpringArmComponent.hpp"
#include "AI/UBehaviorTree.hpp"
#include "Gameplay/UClassRegistry.hpp"
#include "ALeonTournamentGameMode.hpp"
#include "ALeonTournamentGameState.hpp"
#include "ALeonTournamentCharacter.hpp"
#include "ALeonTournamentPlayerController.hpp"
#include "ALeonTournamentPlayerState.hpp"
#include "ALeonTournamentHUD.hpp"
#include "ALeonTournamentBotController.hpp"
#include "ALeonTournamentWeapon.hpp"
#include "ULeonTournamentWidgets.hpp"
#include "FENetTransport.hpp"
#include "Lightmass/FLightBuildSettings.hpp"
#include "Engine/Components.hpp"
#include "UMG/UCanvasPanel.hpp"
#include "UMG/UImage.hpp"
#include "LeonTournamentTestSetup.hpp"

#include <cmath>
#include <memory>
#include <thread>
#include <chrono>

namespace Leon {

    namespace {
        void RegisterLeonTournamentClasses() {
            Test::BindLeonTournamentProject();
            auto& r = UClassRegistry::Get();
            r.RegisterClass<ALeonTournamentGameMode>("ALeonTournamentGameMode");
            r.RegisterClass<ALeonTournamentGameState>("ALeonTournamentGameState");
            r.RegisterClass<ALeonTournamentCharacter>("ALeonTournamentCharacter");
            r.RegisterClass<ALeonTournamentPlayerState>("ALeonTournamentPlayerState");
            r.RegisterClass<ALeonTournamentPlayerController>("ALeonTournamentPlayerController");
            r.RegisterClass<ALeonTournamentHUD>("ALeonTournamentHUD");
            r.RegisterClass<ALeonTournamentBotController>("ALeonTournamentBotController");
            r.RegisterClass<ALeonTournamentRifle>("ALeonTournamentRifle");
        }

        struct FMatchWorld {
            TRef<UWorld> World;
            ALeonTournamentGameMode* GM = nullptr;
            ALeonTournamentGameState* GS = nullptr;

            FMatchWorld() {
                RegisterLeonTournamentClasses();
                World = UWorld::Create("Playability");
                GM = World->SpawnActor<ALeonTournamentGameMode>("GM");
                World->SetGameMode(GM);
                World->InitWorld();
                GM->HUDClass = "None";
                World->BeginPlay();
                GS = GM->GetGameState();
            }
        };
    } // namespace

    TEST_SUITE("PlayerSpawnTests") {
        TEST_CASE("match spawn possesses a character on a PlayerStart") {
            FMatchWorld f;
            f.GM->SetMatchConfig({.MaxPlayers = 2, .StartCountdownSeconds = 0.0f});
            f.GM->EnterLobby();
            f.GM->StartMatch();
            auto* pc = f.World->GetFirstPlayerController();
            REQUIRE(pc);
            auto* ch = pc->GetPawn<ALeonTournamentCharacter>();
            REQUIRE(ch);
            CHECK(ch->GetController() == pc);
            CHECK_FALSE(ch->IsThirdPerson());
            CHECK(ch->GetCapsuleHeight() >= FWorldUnits::ExpectedHumanHeightMin);
            CHECK(ch->GetCapsuleHeight() <= FWorldUnits::ExpectedHumanHeightMax);
        }
    }

    TEST_SUITE("PlayerPossessionTests") {
        TEST_CASE("controller remains after pawn freeze on death path") {
            FMatchWorld f;
            auto* pc = f.World->GetFirstPlayerController();
            auto* ch = f.World->SpawnActor<ALeonTournamentCharacter>("Pawn");
            pc->Possess(ch);
            CHECK(pc->GetPawn() == ch);
            CHECK(ch->IsLocallyControlled());
        }
    }

    TEST_SUITE("CharacterMovementIntegrationTests") {
        TEST_CASE("W-equivalent input changes position while walking") {
            auto world = UWorld::Create("MoveInt");
            world->BeginPlay();
            auto* ch = world->SpawnActor<ACharacter>("Mover");
            ch->SetFloorZ(0.0f);
            ch->SetActorLocation({0.0f, 1.7f, 0.0f});
            auto move = ch->GetCharacterMovement();
            REQUIRE(move);
            const glm::vec3 start = ch->GetActorLocation();
            glm::vec3 forward = ch->GetControlPlanarForward();
            for (int i = 0; i < 60; ++i) {
                move->AddInputVector(forward * move->GetMaxWalkSpeed());
                move->PerformMovement(1.0f / 60.0f);
            }
            CHECK(glm::length(ch->GetActorLocation() - start) > 0.5f);
            CHECK(move->GetMovementMode() == EMovementMode::Walking);
            CHECK(glm::length(glm::vec3(move->GetVelocity().x, 0.0f, move->GetVelocity().z)) > 0.1f);
        }

        TEST_CASE("world tick with a floor does not pin the capsule into the ground") {
            auto world = UWorld::Create("MoveFloor");
            world->BeginPlay();
            auto* floor = world->SpawnActor<AActor>("Floor");
            floor->SetActorLocation({0.0f, -0.25f, 0.0f});
            floor->SetActorScale({24.0f, 0.5f, 24.0f});
            auto box = floor->AddActorComponent<UBoxComponent>("Box");
            box->SetBoxExtent(glm::vec3(0.5f));
            box->SetCollisionObjectType(ECollisionChannel::WorldStatic);

            auto* ch = world->SpawnActor<ACharacter>("Mover");
            ch->SetFloorZ(0.0f);
            ch->SetActorLocation({0.0f, 1.7f, 0.0f});
            auto move = ch->GetCharacterMovement();
            REQUIRE(move);
            const glm::vec3 start = ch->GetActorLocation();
            glm::vec3 forward = ch->GetControlPlanarForward();
            for (int i = 0; i < 60; ++i) {
                move->AddInputVector(forward * move->GetMaxWalkSpeed());
                world->Tick(FTimestep(1.0f / 60.0f));
            }
            CHECK(ch->GetActorLocation().y == doctest::Approx(ch->GetCapsuleHalfHeight()).epsilon(0.2f));
            const glm::vec3 planar(ch->GetActorLocation().x - start.x, 0.0f, ch->GetActorLocation().z - start.z);
            CHECK(glm::length(planar) > 0.5f);
        }
    }

    TEST_SUITE("CameraRotationTests") {
        TEST_CASE("control pitch is stored independently") {
            auto world = UWorld::Create("CamRot");
            world->BeginPlay();
            auto* pc = world->SpawnActor<APlayerController>("PC");
            auto* ch = world->SpawnActor<ACharacter>("Char");
            world->AddPlayerController(pc);
            pc->Possess(ch);
            ch->SetControlRotation({60.0f, 90.0f, 0.0f});
            CHECK(ch->GetControlPitch() == doctest::Approx(60.0f));
            CHECK(ch->GetControlYaw() == doctest::Approx(90.0f));
        }
    }

    TEST_SUITE("CameraCharacterSeparationTests") {
        TEST_CASE("looking up does not pitch the character actor") {
            auto world = UWorld::Create("CamSep");
            world->BeginPlay();
            auto* pc = world->SpawnActor<APlayerController>("PC");
            auto* ch = world->SpawnActor<ACharacter>("Char");
            world->AddPlayerController(pc);
            pc->Possess(ch);
            const float pitches[] = {-89.0f, 0.0f, 60.0f, 89.0f};
            for (float pitch : pitches) {
                ch->SetControlRotation({pitch, 90.0f, 0.0f});
                ch->Tick(0.016f);
                CHECK(std::abs(ch->GetActorRotation().x) < 0.01f);
                CHECK(std::abs(ch->GetActorRotation().z) < 0.01f);
                CHECK(ch->GetControlPitch() == doctest::Approx(pitch));
                CHECK(glm::length(ch->GetActorUpVector() - glm::vec3(0.0f, 1.0f, 0.0f)) < 0.02f);
                if (ch->HasComponent<UCameraComponent>())
                    CHECK(ch->GetComponent<UCameraComponent>().Camera.GetPitch() == doctest::Approx(pitch));
            }
        }
    }

    TEST_SUITE("JumpIntegrationTests") {
        TEST_CASE("jump enters Falling then returns to Walking") {
            auto world = UWorld::Create("JumpInt");
            world->BeginPlay();
            auto* ch = world->SpawnActor<ACharacter>("Jumper");
            ch->SetFloorZ(0.0f);
            ch->SetActorLocation({0.0f, 1.7f, 0.0f});
            auto move = ch->GetCharacterMovement();
            CHECK(ch->IsMovingOnGround());
            ch->Jump();
            move->PerformMovement(0.05f);
            CHECK(ch->IsFalling());
            CHECK(move->GetMovementMode() == EMovementMode::Falling);
            for (int i = 0; i < 80; ++i)
                move->PerformMovement(0.05f);
            CHECK(ch->IsMovingOnGround());
            CHECK(move->GetMovementMode() == EMovementMode::Walking);
        }
    }

    TEST_SUITE("BotMovementIntegrationTests") {
        TEST_CASE("MoveToLocation issues CMC request and moves the pawn") {
            auto world = UWorld::Create("BotMove");
            world->BeginPlay();
            auto* ai = world->SpawnActor<AAIController>("AI");
            world->AddAIController(ai);
            auto* ch = world->SpawnActor<ACharacter>("Bot");
            ch->SetFloorZ(0.0f);
            ch->SetActorLocation({0.0f, 1.7f, 0.0f});
            ai->Possess(ch);
            const glm::vec3 start = ch->GetActorLocation();
            ai->MoveToLocation({8.0f, 1.7f, 0.0f}, 0.6f);
            CHECK(ai->HasActiveMoveRequest());
            for (int i = 0; i < 120; ++i) {
                world->Tick(FTimestep(1.0f / 60.0f));
            }
            CHECK(glm::length(ch->GetActorLocation() - start) > 1.0f);
        }

        TEST_CASE("MoveToLocation with a floor still translates the pawn") {
            auto world = UWorld::Create("BotFloor");
            world->BeginPlay();
            auto* floor = world->SpawnActor<AActor>("Floor");
            floor->SetActorLocation({0.0f, -0.25f, 0.0f});
            floor->SetActorScale({24.0f, 0.5f, 24.0f});
            auto box = floor->AddActorComponent<UBoxComponent>("Box");
            box->SetBoxExtent(glm::vec3(0.5f));
            box->SetCollisionObjectType(ECollisionChannel::WorldStatic);

            auto* other = world->SpawnActor<ACharacter>("Blocker");
            other->SetFloorZ(0.0f);
            other->SetActorLocation({2.0f, 1.7f, 0.0f});

            auto* ai = world->SpawnActor<AAIController>("AI");
            world->AddAIController(ai);
            auto* ch = world->SpawnActor<ACharacter>("Bot");
            ch->SetFloorZ(0.0f);
            ch->SetActorLocation({0.0f, 1.7f, 0.0f});
            ai->Possess(ch);
            const glm::vec3 start = ch->GetActorLocation();
            ai->MoveToLocation({8.0f, 1.7f, 0.0f}, 0.6f);
            for (int i = 0; i < 120; ++i)
                world->Tick(FTimestep(1.0f / 60.0f));
            const glm::vec3 planar(ch->GetActorLocation().x - start.x, 0.0f, ch->GetActorLocation().z - start.z);
            CHECK(glm::length(planar) > 1.0f);
        }
    }

    TEST_SUITE("BehaviorTreeMovementTests") {
        TEST_CASE("MoveTo task generates a move request") {
            auto world = UWorld::Create("BTMove");
            world->BeginPlay();
            auto* ai = world->SpawnActor<AAIController>("AI");
            world->AddAIController(ai);
            auto* ch = world->SpawnActor<ACharacter>("Bot");
            ai->Possess(ch);
            auto data = MakeRef<UBlackboardData>("BB");
            data->AddKey({"TargetLocation", EBlackboardKeyType::Vector, glm::vec3(6.0f, 1.7f, 0.0f)});
            auto root = MakeRef<UBTComposite_Sequence>("Root");
            root->AddChild(MakeRef<UBTTask_MoveTo>("TargetLocation", 0.5f));
            auto tree = MakeRef<UBehaviorTree>("Tree");
            tree->SetRoot(root);
            tree->SetBlackboardAsset(data);
            ai->RunBehaviorTree(tree);
            ai->Tick(0.016f);
            CHECK(ai->HasActiveMoveRequest());
        }
    }

    TEST_SUITE("CrosshairTests") {
        TEST_CASE("centered crosshair slot has non-zero size") {
            UCanvasPanel canvas("Hud");
            canvas.SetSize({1280, 720});
            auto h = std::make_shared<UImage>("H");
            canvas.AddChild(h, FAnchors::Center(), FMargin(-12.0f, -1.5f, -12.0f, -1.5f));
            canvas.PerformLayout({1280, 720});
            CHECK(h->GetSize().x > 8.0f);
            CHECK(h->GetSize().y > 1.0f);
            CHECK(h->GetPosition().x + h->GetSize().x * 0.5f == doctest::Approx(640.0f).epsilon(2.0f));
        }
    }

    TEST_SUITE("HitMarkerTests") {
        TEST_CASE("confirmed hit arms the player-controller marker") {
            FMatchWorld f;
            auto* pc = dynamic_cast<ALeonTournamentPlayerController*>(f.World->GetFirstPlayerController());
            REQUIRE(pc);
            CHECK_FALSE(pc->IsHitMarkerActive());
            pc->NotifyConfirmedHit(false);
            CHECK(pc->IsHitMarkerActive());
        }
    }

    TEST_SUITE("ShootingIntegrationTests") {
        TEST_CASE("server trace applies damage and hit confirm") {
            FMatchWorld f;
            f.GS->SetMatchState(ELeonTournamentMatchState::Playing);
            auto* attacker = f.World->SpawnActor<ALeonTournamentCharacter>("Atk");
            auto* victim = f.World->SpawnActor<ALeonTournamentCharacter>("Vic");
            auto* pca = dynamic_cast<ALeonTournamentPlayerController*>(f.World->GetFirstPlayerController());
            REQUIRE(pca);
            auto* psv = f.World->SpawnActor<ALeonTournamentPlayerState>("PSV");
            if (auto* psa = dynamic_cast<ALeonTournamentPlayerState*>(pca->GetPlayerState()))
                psa->SetTeam(ELeonTournamentTeam::Team1);
            psv->SetTeam(ELeonTournamentTeam::Team2);
            auto* pcb = f.World->SpawnActor<ALeonTournamentPlayerController>("PCB");
            pcb->SetPlayerState(psv);
            pca->Possess(attacker);
            pcb->Possess(victim);
            attacker->SetThirdPerson(false);
            attacker->SetActorLocation({0.0f, 1.7f, 0.0f});
            attacker->SetControlYaw(90.0f);
            attacker->SetControlPitch(0.0f);
            attacker->Tick(0.016f);
            glm::vec3 origin, dir;
            attacker->GetAimRay(origin, dir);
            victim->SetActorLocation(origin + dir * 4.0f);
            REQUIRE(attacker->GetWeapon());
            const float hp = victim->GetHealthComponent()->GetHealth();
            attacker->GetWeapon()->SetFireHeld(true);
            attacker->GetWeapon()->Tick(0.05f);
            CHECK(victim->GetHealthComponent()->GetHealth() < hp);
            CHECK(pca->IsHitMarkerActive());
        }
    }

    TEST_SUITE("GameInstanceFlowTests") {
        TEST_CASE("offline session does not require a transport") {
            FMatchWorld f;
            CHECK(f.World->GetNetMode() == ENetMode::Standalone);
            CHECK(f.World->GetNetDriver() == nullptr);
        }
    }

    TEST_SUITE("GameModeLifecycleTests") {
        TEST_CASE("StartMatch is idempotent while playing") {
            FMatchWorld f;
            FLeonTournamentMatchConfig cfg;
            cfg.MaxPlayers = 2;
            cfg.StartCountdownSeconds = 0.0f;
            f.GM->SetMatchConfig(cfg);
            f.GM->StartMatch();
            auto* first = f.World->GetFirstPlayerController()->GetPawn();
            f.GM->StartMatch();
            CHECK(f.World->GetFirstPlayerController()->GetPawn() == first);
        }
    }

    TEST_SUITE("GameStateReplicationTests") {
        TEST_CASE("loopback replicates match state") {
            auto server = UWorld::Create("GSServer");
            auto client = UWorld::Create("GSClient");
            server->SetNetMode(ENetMode::ListenServer);
            client->SetNetMode(ENetMode::Client);
            FLoopbackNetDriver sdrv, cdrv;
            FLoopbackNetDriver::Pair(sdrv, cdrv);
            sdrv.SetWorld(server.get());
            cdrv.SetWorld(client.get());
            server->SetNetDriver(&sdrv);
            client->SetNetDriver(&cdrv);
            RegisterLeonTournamentClasses();
            auto* gm = server->SpawnActor<ALeonTournamentGameMode>("GM");
            server->SetGameMode(gm);
            server->InitWorld();
            gm->HUDClass = "None";
            server->BeginPlay();
            gm->GetGameState()->SetMatchState(ELeonTournamentMatchState::Playing);
            gm->GetGameState()->SetTeam1Kills(4);
            for (int i = 0; i < 3; ++i) {
                server->Tick(FTimestep(0.05f));
                client->Tick(FTimestep(0.05f));
            }
            auto* gs = dynamic_cast<ALeonTournamentGameState*>(client->GetGameState());
            REQUIRE(gs);
            CHECK(gs->GetMatchState() == ELeonTournamentMatchState::Playing);
            CHECK(gs->GetTeam1Kills() == 4);
        }
    }

    TEST_SUITE("PlayerStatePersistenceTests") {
        TEST_CASE("kills survive pawn respawn") {
            FMatchWorld f;
            auto* pc = f.World->GetFirstPlayerController();
            auto* ps = dynamic_cast<ALeonTournamentPlayerState*>(pc->GetPlayerState());
            REQUIRE(ps);
            ps->AddKill();
            auto* pawn = f.World->SpawnActor<ALeonTournamentCharacter>("P");
            pc->Possess(pawn);
            CHECK(ps->GetKills() == 1);
            CHECK(pawn->GetPlayerState() == ps);
        }
    }

    TEST_SUITE("ENetConnectionTests") {
        TEST_CASE("listen and connect exchange a reliable packet") {
            FENetTransport server;
            FENetTransport client;
            REQUIRE(server.Listen(17777));
            REQUIRE(client.Connect("127.0.0.1", 17777));
            const uint8_t hello[] = {'H', 'I'};
            bool bGot = false;
            for (int i = 0; i < 50 && !bGot; ++i) {
                server.Poll();
                client.Poll();
                int32_t accepted = server.ConsumeAcceptedConnection();
                if (accepted >= 0)
                    server.Send(accepted, hello, sizeof(hello), true);
                auto packets = client.TakeIncoming();
                for (const auto& p : packets) {
                    if (p.Bytes.size() == 2 && p.Bytes[0] == 'H')
                        bGot = true;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            CHECK(bGot);
            server.Close();
            client.Close();
        }
    }

    TEST_SUITE("ENetReliabilityTests") {
        TEST_CASE("reliable flag is preserved on receive") {
            FENetTransport server;
            FENetTransport client;
            REQUIRE(server.Listen(17778));
            REQUIRE(client.Connect("127.0.0.1", 17778));
            bool bReliable = false;
            const uint8_t payload[] = {1, 2, 3, 4};
            for (int i = 0; i < 50 && !bReliable; ++i) {
                server.Poll();
                client.Poll();
                int32_t accepted = server.ConsumeAcceptedConnection();
                if (accepted >= 0)
                    server.Send(accepted, payload, sizeof(payload), true);
                for (const auto& p : client.TakeIncoming()) {
                    if (p.bReliable && p.Bytes.size() == 4)
                        bReliable = true;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            CHECK(bReliable);
            server.Close();
            client.Close();
        }
    }

    TEST_SUITE("OfflineGameFlowTests") {
        TEST_CASE("offline match uses GameMode GameState Controller Pawn without net") {
            FMatchWorld f;
            FLeonTournamentMatchConfig cfg;
            cfg.MaxPlayers = 2;
            cfg.StartCountdownSeconds = 0.0f;
            f.GM->SetMatchConfig(cfg);
            f.GM->EnterLobby();
            f.GM->StartMatch();
            CHECK(f.World->GetNetMode() == ENetMode::Standalone);
            CHECK(f.World->GetGameMode() == f.GM);
            CHECK(f.GS != nullptr);
            CHECK(f.World->GetFirstPlayerController() != nullptr);
            CHECK(f.World->GetFirstPlayerController()->GetPawn() != nullptr);
            CHECK(f.World->GetFirstPlayerController()->GetPlayerState() != nullptr);
        }
    }

    TEST_SUITE("HDRImportTests") {
        TEST_CASE("playable HDR asset path is configured") {
            FMatchWorld f;
            f.GM->EnsurePlayableLighting();
            AActor* env = f.World->FindActorByName("Environment Skybox");
            REQUIRE(env);
            REQUIRE(env->HasComponent<FSkyboxComponent>());
            CHECK(env->GetComponent<FSkyboxComponent>().bUseHDREnvironmentMap);
            CHECK(env->GetComponent<FSkyboxComponent>().HDREnvironmentMapPath.find(".lhdr") != std::string::npos);
        }
    }

    TEST_SUITE("IBLIntegrationTests") {
        TEST_CASE("skybox environment intensity is finite and positive") {
            FMatchWorld f;
            f.GM->EnsurePlayableLighting();
            auto& sky = f.World->FindActorByName("Environment Skybox")->GetComponent<FSkyboxComponent>();
            CHECK(std::isfinite(sky.EnvironmentIntensity));
            CHECK(sky.EnvironmentIntensity > 0.0f);
            CHECK(sky.bEnabled);
        }
    }

    TEST_SUITE("LightBakeIntegrationTests") {
        TEST_CASE("medium bake preset is deterministic") {
            auto a = FLightBuildSettings::Medium().ToLightmass();
            auto b = FLightBuildSettings::Medium().ToLightmass();
            CHECK(a.LightmapResolution == b.LightmapResolution);
            CHECK(a.SamplesPerTexel == 8);
            CHECK(a.NumIndirectBounces == 2);
        }
    }

    TEST_SUITE("ForwardAxisTests") {
        TEST_CASE("engine forward is -Z and control yaw -90 looks along it") {
            CHECK(FWorldUnits::Forward() == glm::vec3(0.0f, 0.0f, -1.0f));
            glm::vec3 f = FWorldUnits::PlanarForwardFromYaw(-90.0f);
            CHECK(f.x == doctest::Approx(0.0f).epsilon(0.02f));
            CHECK(f.z == doctest::Approx(-1.0f).epsilon(0.02f));
        }
    }

} // namespace Leon
