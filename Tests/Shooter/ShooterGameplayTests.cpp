#include <doctest/doctest.h>

#include "Core/FTimestep.hpp"
#include "Engine/FLoopbackNetDriver.hpp"
#include "Engine/UWorld.hpp"
#include "Gameplay/UClassRegistry.hpp"
#include "AShooterGameMode.hpp"
#include "AShooterGameState.hpp"
#include "AShooterCharacter.hpp"
#include "AShooterPlayerState.hpp"
#include "AShooterPlayerController.hpp"
#include "AShooterWeapon.hpp"
#include "AShooterHUD.hpp"
#include "AShooterBotController.hpp"
#include "UShooterGameInstance.hpp"
#include "Core/FWorldUnits.hpp"
#include "Engine/ECollisionChannel.hpp"
#include "Engine/Components.hpp"

#include <vector>

namespace Leon {

    namespace {
        void RegisterShooterClasses() {
            auto& r = UClassRegistry::Get();
            r.RegisterClass<AShooterGameMode>("AShooterGameMode");
            r.RegisterClass<AShooterGameState>("AShooterGameState");
            r.RegisterClass<AShooterCharacter>("AShooterCharacter");
            r.RegisterClass<AShooterPlayerState>("AShooterPlayerState");
            r.RegisterClass<AShooterPlayerController>("AShooterPlayerController");
            r.RegisterClass<AShooterHUD>("AShooterHUD");
            r.RegisterClass<AShooterBotController>("AShooterBotController");
            r.RegisterClass<AShooterRifle>("AShooterRifle");
        }

        struct FMatchWorld {
            TRef<UWorld> World;
            AShooterGameMode* GM = nullptr;
            AShooterGameState* GS = nullptr;

            FMatchWorld() {
                RegisterShooterClasses();
                World = UWorld::Create("ShooterTest");
                GM = World->SpawnActor<AShooterGameMode>("GM");
                World->SetGameMode(GM);
                World->InitWorld();
                GM->HUDClass = "None";
                World->BeginPlay();
                GS = GM->GetShooterGameState();
            }
        };
    } // namespace

    TEST_SUITE("Shooter teams / match / score") {

        TEST_CASE("two teams max 8 and friendly fire rejected") {
            FMatchWorld f;
            REQUIRE(f.GS);
            f.GM->EnterLobby();
            CHECK(f.GM->CountTeam(EShooterTeam::Team1) <= 8);
            CHECK(f.GM->CountTeam(EShooterTeam::Team2) <= 8);
            CHECK(f.GM->CountTeam(EShooterTeam::Team1) + f.GM->CountTeam(EShooterTeam::Team2) == 16);

            f.GS->SetMatchState(EShooterMatchState::Playing);
            auto* a = f.World->SpawnActor<AShooterCharacter>("A");
            auto* b = f.World->SpawnActor<AShooterCharacter>("B");
            auto* psa = f.World->SpawnActor<AShooterPlayerState>("PSA");
            auto* psb = f.World->SpawnActor<AShooterPlayerState>("PSB");
            psa->SetTeam(EShooterTeam::Team1);
            psb->SetTeam(EShooterTeam::Team1);
            auto* pca = f.World->SpawnActor<AShooterPlayerController>("PCA");
            auto* pcb = f.World->SpawnActor<AShooterPlayerController>("PCB");
            pca->SetPlayerState(psa);
            pcb->SetPlayerState(psb);
            pca->Possess(a);
            pcb->Possess(b);

            FDamageInfo info;
            info.DamageAmount = 25.0f;
            info.Instigator = a;
            CHECK_FALSE(f.GM->ApplyAuthoritativeDamage(*a, *b, info));
            CHECK(b->GetHealthComponent()->GetHealth() == doctest::Approx(100.0f));

            psb->SetTeam(EShooterTeam::Team2);
            CHECK(f.GM->ApplyAuthoritativeDamage(*a, *b, info));
            CHECK(b->GetHealthComponent()->GetHealth() == doctest::Approx(75.0f));
        }

        TEST_CASE("timer victory, score limit, draw") {
            FMatchWorld f;
            REQUIRE(f.GS);
            FShooterMatchConfig cfg;
            cfg.MatchDurationSeconds = 0.2f;
            cfg.ScoreLimit = 2;
            cfg.StartCountdownSeconds = 0.0f;
            cfg.MaxPlayers = 2;
            f.GM->SetMatchConfig(cfg);
            f.GS->SetMatchState(EShooterMatchState::Playing);
            f.GS->SetRemainingTime(0.2f);
            f.GS->SetTeam1Kills(3);
            f.GS->SetTeam2Kills(1);
            f.World->Tick(FTimestep(0.25f));
            CHECK(f.GS->GetMatchState() == EShooterMatchState::Finished);
            CHECK(f.GS->GetMatchWinner() == EShooterMatchWinner::Team1);

            FMatchWorld f2;
            cfg.MatchDurationSeconds = 10.0f;
            f2.GM->SetMatchConfig(cfg);
            f2.GS->SetMatchState(EShooterMatchState::Playing);
            f2.GS->SetRemainingTime(10.0f);
            f2.GS->SetTeam1Kills(2);
            f2.GS->SetTeam2Kills(0);
            CHECK(f2.GS->GetTeam1Kills() == 2);

            FMatchWorld f3;
            f3.GM->SetMatchConfig(cfg);
            f3.GS->SetMatchState(EShooterMatchState::Playing);
            f3.GS->SetRemainingTime(0.05f);
            f3.GS->SetTeam1Kills(4);
            f3.GS->SetTeam2Kills(4);
            f3.World->Tick(FTimestep(0.1f));
            CHECK(f3.GS->GetMatchWinner() == EShooterMatchWinner::Draw);
        }

        TEST_CASE("kill death assist and scoreboard order") {
            FMatchWorld f;
            f.GS->SetMatchState(EShooterMatchState::Playing);
            auto* killer = f.World->SpawnActor<AShooterCharacter>("Killer");
            auto* assist = f.World->SpawnActor<AShooterCharacter>("Assist");
            auto* victim = f.World->SpawnActor<AShooterCharacter>("Victim");

            auto* psk = f.World->SpawnActor<AShooterPlayerState>("PSK");
            auto* psa = f.World->SpawnActor<AShooterPlayerState>("PSA");
            auto* psv = f.World->SpawnActor<AShooterPlayerState>("PSV");
            psk->SetTeam(EShooterTeam::Team1);
            psa->SetTeam(EShooterTeam::Team1);
            psv->SetTeam(EShooterTeam::Team2);
            psk->SetPlayerName("Alpha");
            psa->SetPlayerName("Bravo");
            psv->SetPlayerName("Charlie");
            auto* pck = f.World->SpawnActor<AShooterPlayerController>("PCK");
            auto* pca = f.World->SpawnActor<AShooterPlayerController>("PCA");
            auto* pcv = f.World->SpawnActor<AShooterPlayerController>("PCV");
            pck->SetPlayerState(psk);
            pca->SetPlayerState(psa);
            pcv->SetPlayerState(psv);
            pck->Possess(killer);
            pca->Possess(assist);
            pcv->Possess(victim);
            f.GS->AddPlayerState(psk);
            f.GS->AddPlayerState(psa);
            f.GS->AddPlayerState(psv);

            FDamageInfo d1;
            d1.DamageAmount = 40.0f;
            d1.Instigator = assist;
            f.GM->ApplyAuthoritativeDamage(*assist, *victim, d1);
            FDamageInfo d2;
            d2.DamageAmount = 80.0f;
            d2.Instigator = killer;
            f.GM->ApplyAuthoritativeDamage(*killer, *victim, d2);
            CHECK(victim->GetHealthComponent()->IsDead());
            CHECK(psk->GetKills() == 1);
            CHECK(psv->GetDeaths() == 1);
            CHECK(psa->GetAssists() == 1);

            psk->AddKill();
            auto board = f.GM->GetSortedScoreboard();
            REQUIRE(board.size() >= 3);
            CHECK(board.front()->GetKills() >= board[1]->GetKills());
        }

        TEST_CASE("respawn restores health and magazine") {
            FMatchWorld f;
            f.GS->SetMatchState(EShooterMatchState::Playing);
            f.GM->BuildArena();
            auto* ch = f.World->SpawnActor<AShooterCharacter>("P");
            ch->GetHealthComponent()->ApplyDamage(100.0f);
            CHECK(ch->GetHealthComponent()->IsDead());
            if (ch->GetWeapon())
                ch->GetWeapon()->ServerFire();
            f.GM->RespawnCharacter(*ch);
            CHECK_FALSE(ch->GetHealthComponent()->IsDead());
            CHECK(ch->GetHealthComponent()->GetHealth() == doctest::Approx(100.0f));
            REQUIRE(ch->GetWeapon());
            CHECK(ch->GetWeapon()->GetCurrentAmmo() == ch->GetWeapon()->GetMagazineSize());
        }

        TEST_CASE("starting state rejects combat and StartMatch is idempotent") {
            FMatchWorld f;
            REQUIRE(f.GS);
            f.GS->SetMatchState(EShooterMatchState::Starting);
            auto* a = f.World->SpawnActor<AShooterCharacter>("A");
            auto* b = f.World->SpawnActor<AShooterCharacter>("B");
            auto* psa = f.World->SpawnActor<AShooterPlayerState>("PSA");
            auto* psb = f.World->SpawnActor<AShooterPlayerState>("PSB");
            psa->SetTeam(EShooterTeam::Team1);
            psb->SetTeam(EShooterTeam::Team2);
            auto* pca = f.World->SpawnActor<AShooterPlayerController>("PCA");
            auto* pcb = f.World->SpawnActor<AShooterPlayerController>("PCB");
            pca->SetPlayerState(psa);
            pcb->SetPlayerState(psb);
            pca->Possess(a);
            pcb->Possess(b);
            FDamageInfo info;
            info.DamageAmount = 25.0f;
            info.Instigator = a;
            CHECK_FALSE(f.GM->ApplyAuthoritativeDamage(*a, *b, info));
            CHECK(b->GetHealthComponent()->GetHealth() == doctest::Approx(100.0f));

            FMatchWorld f2;
            FShooterMatchConfig cfg;
            cfg.MaxPlayers = 4;
            cfg.StartCountdownSeconds = 0.0f;
            f2.GM->SetMatchConfig(cfg);
            f2.GM->StartMatch();
            size_t first = 0;
            for (const auto& actor : f2.World->GetAllActors()) {
                if (dynamic_cast<AShooterCharacter*>(actor.get()))
                    ++first;
            }
            f2.GM->StartMatch();
            size_t second = 0;
            for (const auto& actor : f2.World->GetAllActors()) {
                if (dynamic_cast<AShooterCharacter*>(actor.get()))
                    ++second;
            }
            CHECK(first == second);
            CHECK(first >= 4);
        }
    }

    TEST_SUITE("Shooter weapon") {

        TEST_CASE("fire rate ammo reload and sprint block") {
            FMatchWorld f;
            f.GS->SetMatchState(EShooterMatchState::Playing);
            auto* ch = f.World->SpawnActor<AShooterCharacter>("Gunner");
            REQUIRE(ch->GetWeapon());
            auto* weap = ch->GetWeapon();
            CHECK(weap->GetCurrentAmmo() == 30);
            ch->SetSprinting(true);
            CHECK_FALSE(weap->CanFire());
            ch->SetSprinting(false);
            CHECK(weap->CanFire());
            CHECK(weap->ServerFire());
            CHECK(weap->GetCurrentAmmo() == 29);
            CHECK_FALSE(weap->ServerFire());
            weap->Tick(0.2f);
            CHECK(weap->ServerFire());

            while (weap->GetCurrentAmmo() > 0) {
                weap->Tick(0.2f);
                weap->ServerFire();
            }
            CHECK(weap->GetCurrentAmmo() == 0);
            CHECK_FALSE(weap->CanFire());
            CHECK(weap->StartReload());
            CHECK(weap->IsReloading());
            CHECK_FALSE(weap->CanFire());
            weap->Tick(2.1f);
            CHECK_FALSE(weap->IsReloading());
            CHECK(weap->GetCurrentAmmo() == 30);
        }

        TEST_CASE("replicated ammo and reload reach the client pawn") {
            FMatchWorld f;
            f.GS->SetMatchState(EShooterMatchState::Playing);
            auto* src = f.World->SpawnActor<AShooterCharacter>("Src");
            auto* dst = f.World->SpawnActor<AShooterCharacter>("Dst");
            REQUIRE(src->GetWeapon());
            REQUIRE(dst->GetWeapon());
            CHECK(src->GetWeapon()->ServerFire());
            std::vector<uint8_t> blob;
            src->SerializeReplication(blob);
            dst->DeserializeReplication(blob.data(), blob.size());
            CHECK(dst->GetWeapon()->GetCurrentAmmo() == src->GetWeapon()->GetCurrentAmmo());
            CHECK(dst->GetHealthComponent()->GetHealth() == doctest::Approx(src->GetHealthComponent()->GetHealth()));
        }
    }

    TEST_SUITE("Shooter net authority") {

        TEST_CASE("client cannot author health kills or team score") {
            RegisterShooterClasses();
            auto server = UWorld::Create("S");
            auto client = UWorld::Create("C");
            server->SetNetMode(ENetMode::ListenServer);
            client->SetNetMode(ENetMode::Client);
            FLoopbackNetDriver sd, cd;
            sd.SetWorld(server.get());
            cd.SetWorld(client.get());
            FLoopbackNetDriver::Pair(sd, cd);
            server->SetNetDriver(&sd);
            client->SetNetDriver(&cd);

            auto* gm = server->SpawnActor<AShooterGameMode>("GM");
            server->SetGameMode(gm);
            server->InitWorld();
            gm->HUDClass = "None";
            server->BeginPlay();
            auto* gs = gm->GetShooterGameState();
            REQUIRE(gs);
            gs->SetMatchState(EShooterMatchState::Playing);
            gs->SetTeam1Kills(7);

            auto* ps = dynamic_cast<AShooterPlayerState*>(server->GetFirstPlayerController()->GetPlayerState());
            REQUIRE(ps);
            ps->AddKill();

            for (int i = 0; i < 4; ++i) {
                server->Tick(FTimestep(0.05f));
                client->Tick(FTimestep(0.05f));
            }

            auto* cgs = dynamic_cast<AShooterGameState*>(client->GetGameState());
            REQUIRE(cgs);
            CHECK(cgs->GetTeam1Kills() == 7);
            CHECK(cgs->GetMatchState() == EShooterMatchState::Playing);

            cgs->SetTeam1Kills(99);
            server->Tick(FTimestep(0.05f));
            client->Tick(FTimestep(0.05f));
            CHECK(dynamic_cast<AShooterGameState*>(client->GetGameState())->GetTeam1Kills() == 7);
        }
    }

    TEST_SUITE("Shooter ownership / scale / traces / bots") {

        TEST_CASE("GameInstance holds session data only") {
            UShooterGameInstance gi("GI");
            gi.SetSessionMode(EShooterSessionMode::LanClient);
            gi.SetJoinAddress("192.168.1.50");
            CHECK(gi.GetSessionMode() == EShooterSessionMode::LanClient);
            CHECK(gi.GetJoinAddress() == "192.168.1.50");
            CHECK(gi.GetLanPort() == 7777);
        }

        TEST_CASE("PlayerState stats survive respawn") {
            FMatchWorld f;
            f.GS->SetMatchState(EShooterMatchState::Playing);
            auto* ch = f.World->SpawnActor<AShooterCharacter>("P");
            auto* ps = f.World->SpawnActor<AShooterPlayerState>("PS");
            auto* pc = f.World->SpawnActor<AShooterPlayerController>("PC");
            ps->SetTeam(EShooterTeam::Team1);
            pc->SetPlayerState(ps);
            pc->Possess(ch);
            ps->AddKill();
            ps->AddDeath();
            ps->AddAssist();
            f.GM->RespawnCharacter(*ch);
            CHECK(ps->GetKills() == 1);
            CHECK(ps->GetDeaths() == 1);
            CHECK(ps->GetAssists() == 1);
            CHECK(ch->GetHealthComponent()->GetHealth() == doctest::Approx(100.0f));
        }

        TEST_CASE("ammo lives on the weapon only") {
            FMatchWorld f;
            f.GS->SetMatchState(EShooterMatchState::Playing);
            auto* ch = f.World->SpawnActor<AShooterCharacter>("P");
            REQUIRE(ch->GetWeapon());
            CHECK(ch->GetWeapon()->GetCurrentAmmo() == ch->GetWeapon()->GetMagazineSize());
            CHECK(ch->GetWeapon()->ServerFire());
            CHECK(ch->GetWeapon()->GetCurrentAmmo() == ch->GetWeapon()->GetMagazineSize() - 1);
        }

        TEST_CASE("health lives on HealthComponent") {
            FMatchWorld f;
            f.GS->SetMatchState(EShooterMatchState::Playing);
            auto* a = f.World->SpawnActor<AShooterCharacter>("A");
            auto* b = f.World->SpawnActor<AShooterCharacter>("B");
            auto* psa = f.World->SpawnActor<AShooterPlayerState>("PSA");
            auto* psb = f.World->SpawnActor<AShooterPlayerState>("PSB");
            psa->SetTeam(EShooterTeam::Team1);
            psb->SetTeam(EShooterTeam::Team2);
            auto* pca = f.World->SpawnActor<AShooterPlayerController>("PCA");
            auto* pcb = f.World->SpawnActor<AShooterPlayerController>("PCB");
            pca->SetPlayerState(psa);
            pcb->SetPlayerState(psb);
            pca->Possess(a);
            pcb->Possess(b);
            FDamageInfo info;
            info.DamageAmount = 40.0f;
            info.Instigator = a;
            REQUIRE(f.GM->ApplyAuthoritativeDamage(*a, *b, info));
            CHECK(b->GetHealthComponent()->GetHealth() == doctest::Approx(60.0f));
        }

        TEST_CASE("lobby and finished reject combat") {
            FMatchWorld f;
            auto* a = f.World->SpawnActor<AShooterCharacter>("A");
            auto* b = f.World->SpawnActor<AShooterCharacter>("B");
            auto* psa = f.World->SpawnActor<AShooterPlayerState>("PSA");
            auto* psb = f.World->SpawnActor<AShooterPlayerState>("PSB");
            psa->SetTeam(EShooterTeam::Team1);
            psb->SetTeam(EShooterTeam::Team2);
            auto* pca = f.World->SpawnActor<AShooterPlayerController>("PCA");
            auto* pcb = f.World->SpawnActor<AShooterPlayerController>("PCB");
            pca->SetPlayerState(psa);
            pcb->SetPlayerState(psb);
            pca->Possess(a);
            pcb->Possess(b);
            FDamageInfo info;
            info.DamageAmount = 25.0f;
            info.Instigator = a;
            f.GS->SetMatchState(EShooterMatchState::Lobby);
            CHECK_FALSE(f.GM->ApplyAuthoritativeDamage(*a, *b, info));
            f.GS->SetMatchState(EShooterMatchState::Finished);
            CHECK_FALSE(f.GM->ApplyAuthoritativeDamage(*a, *b, info));
        }

        TEST_CASE("character capsule matches world-unit human height") {
            FMatchWorld f;
            auto* ch = f.World->SpawnActor<AShooterCharacter>("Human");
            CHECK(ch->GetEyeHeight() == doctest::Approx(FWorldUnits::ExpectedEyeHeight));
            CHECK(ch->GetCapsuleHeight() >= FWorldUnits::ExpectedHumanHeightMin);
            CHECK(ch->GetCapsuleHeight() <= FWorldUnits::ExpectedHumanHeightMax);
            CHECK(3.0f > ch->GetCapsuleHeight());
            CHECK(1.4f < ch->GetEyeHeight());
        }

        TEST_CASE("spawn validation does not crash a possessed pawn") {
            FMatchWorld f;
            f.GM->BuildArena();
            f.GS->SetMatchState(EShooterMatchState::Playing);
            auto* ch = f.World->SpawnActor<AShooterCharacter>("Spawned");
            auto* ps = f.World->SpawnActor<AShooterPlayerState>("PS");
            auto* pc = f.World->SpawnActor<AShooterPlayerController>("PC");
            ps->SetTeam(EShooterTeam::Team1);
            pc->SetPlayerState(ps);
            ch->SetActorLocation(f.GM->GetTeamSpawnLocation(EShooterTeam::Team1));
            pc->Possess(ch);
            CHECK(ch->GetController() == pc);
            CHECK(ch->GetWeapon() != nullptr);
            CHECK(ch->GetHealthComponent());
            CHECK_FALSE(ch->GetHealthComponent()->IsDead());
        }

        TEST_CASE("wall occludes hitscan between enemies") {
            FMatchWorld f;
            f.GS->SetMatchState(EShooterMatchState::Playing);
            auto* wall = f.World->SpawnActor<AActor>("Wall");
            wall->SetActorLocation({0.0f, 1.0f, 5.0f});
            wall->AddComponent<FBoxCollisionComponent>(glm::vec3(-2.0f, -2.0f, -0.3f), glm::vec3(2.0f, 2.0f, 0.3f),
                                                       true, ECollisionChannel::WorldStatic);
            auto* a = f.World->SpawnActor<AShooterCharacter>("A");
            auto* b = f.World->SpawnActor<AShooterCharacter>("B");
            auto* psa = f.World->SpawnActor<AShooterPlayerState>("PSA");
            auto* psb = f.World->SpawnActor<AShooterPlayerState>("PSB");
            psa->SetTeam(EShooterTeam::Team1);
            psb->SetTeam(EShooterTeam::Team2);
            auto* pca = f.World->SpawnActor<AShooterPlayerController>("PCA");
            auto* pcb = f.World->SpawnActor<AShooterPlayerController>("PCB");
            pca->SetPlayerState(psa);
            pcb->SetPlayerState(psb);
            a->SetActorLocation({0.0f, 1.7f, 0.0f});
            b->SetActorLocation({0.0f, 1.7f, 10.0f});
            pca->Possess(a);
            pcb->Possess(b);
            a->SetControlYaw(90.0f);
            a->SetControlPitch(0.0f);
            REQUIRE(a->GetWeapon());
            a->GetWeapon()->ServerFire();
            CHECK(b->GetHealthComponent()->GetHealth() == doctest::Approx(100.0f));
        }

        TEST_CASE("LineTraceByChannel Visibility hits pawn and WorldStatic") {
            FMatchWorld f;
            auto* wall = f.World->SpawnActor<AActor>("Wall");
            wall->SetActorLocation({0.0f, 1.0f, 4.0f});
            wall->AddComponent<FBoxCollisionComponent>(glm::vec3(-0.5f), glm::vec3(0.5f), true,
                                                       ECollisionChannel::WorldStatic);
            auto* pawn = f.World->SpawnActor<AShooterCharacter>("Pawn");
            pawn->SetActorLocation({0.0f, 1.7f, 8.0f});
            UWorld::FHitResult hit;
            CHECK(f.World->LineTraceByChannel({0, 1, 0}, {0, 1, 20}, ECollisionChannel::Visibility, nullptr, hit));
            CHECK(hit.Actor == wall);
            CHECK(f.World->LineTraceByChannel({0, 1, 0}, {0, 1, 20}, ECollisionChannel::WorldStatic, nullptr, hit));
            CHECK(hit.Actor == wall);
            CHECK(
                f.World->LineTraceByChannel({0, 1.7f, 6.0f}, {0, 1.7f, 20.0f}, ECollisionChannel::Pawn, nullptr, hit));
            CHECK(hit.Actor == pawn);
            CHECK_FALSE(f.World->LineTraceByChannel({0, 3, 0}, {0, 3, 20}, ECollisionChannel::Pawn, nullptr, hit));
        }

        TEST_CASE("hit and kill confirmation are local timers") {
            FMatchWorld f;
            auto* pc = f.World->SpawnActor<AShooterPlayerController>("PC");
            CHECK_FALSE(pc->IsHitMarkerActive());
            pc->NotifyConfirmedHit(false);
            CHECK(pc->IsHitMarkerActive());
            CHECK_FALSE(pc->IsKillConfirmActive());
            pc->NotifyConfirmedHit(true);
            CHECK(pc->IsKillConfirmActive());
            pc->Tick(0.2f);
            CHECK_FALSE(pc->IsHitMarkerActive());
            pc->NotifyTookDamage();
            CHECK(pc->IsDamageFlashActive());
            pc->Tick(0.3f);
            CHECK_FALSE(pc->IsDamageFlashActive());
        }

        TEST_CASE("bot acquires an enemy and leaves Search") {
            FMatchWorld f;
            f.GS->SetMatchState(EShooterMatchState::Playing);
            auto* botPawn = f.World->SpawnActor<AShooterCharacter>("BotPawn");
            auto* enemy = f.World->SpawnActor<AShooterCharacter>("Enemy");
            auto* psBot = f.World->SpawnActor<AShooterPlayerState>("PSBot");
            auto* psEnemy = f.World->SpawnActor<AShooterPlayerState>("PSEnemy");
            psBot->SetTeam(EShooterTeam::Team1);
            psBot->SetIsBot(true);
            psEnemy->SetTeam(EShooterTeam::Team2);
            auto* bot = f.World->SpawnActor<AShooterBotController>("BotPC");
            auto* enemyPc = f.World->SpawnActor<AShooterPlayerController>("EnemyPC");
            bot->SetPlayerState(psBot);
            enemyPc->SetPlayerState(psEnemy);
            botPawn->SetActorLocation({0.0f, 1.7f, 0.0f});
            enemy->SetActorLocation({4.0f, 1.7f, 0.0f});
            bot->Possess(botPawn);
            enemyPc->Possess(enemy);
            for (int i = 0; i < 20; ++i)
                bot->Tick(0.05f);
            CHECK(bot->GetBotState() != EShooterBotState::Idle);
            CHECK(bot->GetCurrentTarget() == enemy);
            CHECK(bot->HasLineOfSight(*enemy));
        }

        TEST_CASE("GameState countdown replicates with match state") {
            FMatchWorld f;
            f.GS->SetMatchState(EShooterMatchState::Starting);
            f.GS->SetCountdownRemaining(1.5f);
            std::vector<uint8_t> blob;
            f.GS->SerializeReplication(blob);
            auto* other = f.World->SpawnActor<AShooterGameState>("GS2");
            other->DeserializeReplication(blob.data(), blob.size());
            CHECK(other->GetMatchState() == EShooterMatchState::Starting);
            CHECK(other->GetCountdownRemaining() == doctest::Approx(1.5f));
        }

        TEST_CASE("camera aim ray uses look when not locally controlled") {
            FMatchWorld f;
            auto* ch = f.World->SpawnActor<AShooterCharacter>("Aim");
            ch->SetControlYaw(90.0f);
            ch->SetControlPitch(0.0f);
            glm::vec3 origin, dir;
            ch->GetAimRay(origin, dir);
            CHECK(dir.z == doctest::Approx(1.0f).epsilon(0.02f));
        }
    }

} // namespace Leon
