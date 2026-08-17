#include <doctest/doctest.h>

#include "Core/FTimestep.hpp"
#include "Engine/ULoopbackNetDriver.hpp"
#include "Engine/UWorld.hpp"
#include "Gameplay/UClassRegistry.hpp"
#include "ALeonTournamentGameMode.hpp"
#include "ALeonTournamentGameState.hpp"
#include "ALeonTournamentCharacter.hpp"
#include "ALeonTournamentPlayerState.hpp"
#include "ALeonTournamentPlayerController.hpp"
#include "ALeonTournamentWeapon.hpp"
#include "ALeonTournamentProjectile.hpp"
#include "ALeonTournamentPickup.hpp"
#include "ALeonTournamentHUD.hpp"
#include "FLeonTournamentWeaponPresets.hpp"
#include "FLeonTournamentArenaBuilder.hpp"
#include "FLeonTournamentDamageRules.hpp"
#include "Physics/FHitResult.hpp"
#include "ALeonTournamentBotController.hpp"
#include "ULeonTournamentGameInstance.hpp"
#include "Core/FWorldUnits.hpp"
#include "Engine/ECollisionChannel.hpp"
#include "Engine/Components.hpp"
#include "LeonTournamentTestSetup.hpp"

#include <vector>

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
                World = UWorld::Create("LeonTournamentTest");
                GM = World->SpawnActor<ALeonTournamentGameMode>("GM");
                World->SetGameMode(GM);
                World->InitWorld();
                GM->HUDClass = "None";
                World->BeginPlay();
                GS = GM->GetGameState();
            }
        };
    } // namespace

    TEST_SUITE("LeonTournament teams / match / score") {

        TEST_CASE("arena builder spawns box at location") {
            FMatchWorld f;
            AActor* box = FLeonTournamentArenaBuilder::SpawnBox(
                f.World.get(), "TestBox", {1.0f, 2.0f, 3.0f}, {2.0f, 2.0f, 2.0f},
                ELeonTournamentArenaSurface::Prop);
            REQUIRE(box);
            CHECK(glm::length(box->GetActorLocation() - glm::vec3(1.0f, 2.0f, 3.0f)) < 0.01f);
            CHECK_FALSE(box->GetActorComponents().empty());
        }

        TEST_CASE("damage rules block friendly fire when disabled") {
            FMatchWorld f;
            auto* a = f.World->SpawnActor<ALeonTournamentCharacter>("A");
            auto* b = f.World->SpawnActor<ALeonTournamentCharacter>("B");
            auto* psa = f.World->SpawnActor<ALeonTournamentPlayerState>("PSA");
            auto* psb = f.World->SpawnActor<ALeonTournamentPlayerState>("PSB");
            psa->SetTeam(ELeonTournamentTeam::Team1);
            psb->SetTeam(ELeonTournamentTeam::Team1);
            auto* pca = f.World->SpawnActor<ALeonTournamentPlayerController>("PCA");
            auto* pcb = f.World->SpawnActor<ALeonTournamentPlayerController>("PCB");
            pca->SetPlayerState(psa);
            pcb->SetPlayerState(psb);
            pca->Possess(a);
            pcb->Possess(b);
            FLeonTournamentMatchConfig cfg;
            cfg.bFriendlyFire = false;
            CHECK_FALSE(FLeonTournamentDamageRules::CanDamage(cfg, *a, *b));
            cfg.bFriendlyFire = true;
            CHECK(FLeonTournamentDamageRules::CanDamage(cfg, *a, *b));
        }

        TEST_CASE("two teams max 2 and friendly fire rejected") {
            FMatchWorld f;
            REQUIRE(f.GS);
            f.GM->EnterLobby();
            CHECK(f.GM->CountTeam(ELeonTournamentTeam::Team1) <= 2);
            CHECK(f.GM->CountTeam(ELeonTournamentTeam::Team2) <= 2);
            CHECK(f.GM->CountTeam(ELeonTournamentTeam::Team1) + f.GM->CountTeam(ELeonTournamentTeam::Team2) == 4);

            f.GS->SetMatchState(ELeonTournamentMatchState::Playing);
            auto* a = f.World->SpawnActor<ALeonTournamentCharacter>("A");
            auto* b = f.World->SpawnActor<ALeonTournamentCharacter>("B");
            auto* psa = f.World->SpawnActor<ALeonTournamentPlayerState>("PSA");
            auto* psb = f.World->SpawnActor<ALeonTournamentPlayerState>("PSB");
            psa->SetTeam(ELeonTournamentTeam::Team1);
            psb->SetTeam(ELeonTournamentTeam::Team1);
            auto* pca = f.World->SpawnActor<ALeonTournamentPlayerController>("PCA");
            auto* pcb = f.World->SpawnActor<ALeonTournamentPlayerController>("PCB");
            pca->SetPlayerState(psa);
            pcb->SetPlayerState(psb);
            pca->Possess(a);
            pcb->Possess(b);

            FDamageInfo info;
            info.DamageAmount = 25.0f;
            info.Instigator = a;
            CHECK_FALSE(f.GM->ApplyAuthoritativeDamage(*a, *b, info));
            CHECK(b->GetHealthComponent()->GetHealth() == doctest::Approx(100.0f));

            psb->SetTeam(ELeonTournamentTeam::Team2);
            CHECK(f.GM->ApplyAuthoritativeDamage(*a, *b, info));
            CHECK(b->GetHealthComponent()->GetHealth() == doctest::Approx(75.0f));
        }

        TEST_CASE("timer victory, score limit, draw") {
            FMatchWorld f;
            REQUIRE(f.GS);
            FLeonTournamentMatchConfig cfg;
            cfg.MatchDurationSeconds = 0.2f;
            cfg.ScoreLimit = 2;
            cfg.StartCountdownSeconds = 0.0f;
            cfg.MaxPlayers = 2;
            f.GM->SetMatchConfig(cfg);
            f.GS->SetMatchState(ELeonTournamentMatchState::Playing);
            f.GS->SetRemainingTime(0.2f);
            f.GS->SetTeam1Kills(3);
            f.GS->SetTeam2Kills(1);
            f.World->Tick(FTimestep(0.25f));
            CHECK(f.GS->GetMatchState() == ELeonTournamentMatchState::Finished);
            CHECK(f.GS->GetMatchWinner() == ELeonTournamentMatchWinner::Team1);

            FMatchWorld f2;
            cfg.MatchDurationSeconds = 10.0f;
            f2.GM->SetMatchConfig(cfg);
            f2.GS->SetMatchState(ELeonTournamentMatchState::Playing);
            f2.GS->SetRemainingTime(10.0f);
            f2.GS->SetTeam1Kills(2);
            f2.GS->SetTeam2Kills(0);
            CHECK(f2.GS->GetTeam1Kills() == 2);

            FMatchWorld f3;
            f3.GM->SetMatchConfig(cfg);
            f3.GS->SetMatchState(ELeonTournamentMatchState::Playing);
            f3.GS->SetRemainingTime(0.05f);
            f3.GS->SetTeam1Kills(4);
            f3.GS->SetTeam2Kills(4);
            f3.World->Tick(FTimestep(0.1f));
            CHECK(f3.GS->GetMatchWinner() == ELeonTournamentMatchWinner::Draw);
        }

        TEST_CASE("kill death assist and scoreboard order") {
            FMatchWorld f;
            f.GS->SetMatchState(ELeonTournamentMatchState::Playing);
            auto* killer = f.World->SpawnActor<ALeonTournamentCharacter>("Killer");
            auto* assist = f.World->SpawnActor<ALeonTournamentCharacter>("Assist");
            auto* victim = f.World->SpawnActor<ALeonTournamentCharacter>("Victim");

            auto* psk = f.World->SpawnActor<ALeonTournamentPlayerState>("PSK");
            auto* psa = f.World->SpawnActor<ALeonTournamentPlayerState>("PSA");
            auto* psv = f.World->SpawnActor<ALeonTournamentPlayerState>("PSV");
            psk->SetTeam(ELeonTournamentTeam::Team1);
            psa->SetTeam(ELeonTournamentTeam::Team1);
            psv->SetTeam(ELeonTournamentTeam::Team2);
            psk->SetPlayerName("Alpha");
            psa->SetPlayerName("Bravo");
            psv->SetPlayerName("Charlie");
            auto* pck = f.World->SpawnActor<ALeonTournamentPlayerController>("PCK");
            auto* pca = f.World->SpawnActor<ALeonTournamentPlayerController>("PCA");
            auto* pcv = f.World->SpawnActor<ALeonTournamentPlayerController>("PCV");
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
            f.GS->SetMatchState(ELeonTournamentMatchState::Playing);
            f.GM->BuildArena();
            auto* ch = f.World->SpawnActor<ALeonTournamentCharacter>("P");
            auto* ps = f.World->SpawnActor<ALeonTournamentPlayerState>("PS");
            auto* pc = f.World->SpawnActor<ALeonTournamentPlayerController>("PC");
            ps->SetTeam(ELeonTournamentTeam::Team1);
            pc->SetPlayerState(ps);
            pc->Possess(ch);
            ch->GetHealthComponent()->ApplyDamage(100.0f);
            CHECK(ch->GetHealthComponent()->IsDead());
            if (ch->GetWeapon())
                ch->GetWeapon()->ServerFire();
            const FUUID oldGuid = ch->GetActorGuid();
            f.GM->RespawnCharacter(*ch);
            auto* spawned = pc->GetPawn<ALeonTournamentCharacter>();
            REQUIRE(spawned);
            CHECK(spawned->GetActorGuid() != oldGuid);
            CHECK(f.World->FindActorByGuid(oldGuid) == nullptr);
            CHECK_FALSE(spawned->GetHealthComponent()->IsDead());
            CHECK(spawned->GetHealthComponent()->GetHealth() == doctest::Approx(100.0f));
            REQUIRE(spawned->GetWeapon());
            CHECK(spawned->GetWeapon()->GetCurrentAmmo() == spawned->GetWeapon()->GetMagazineSize());
        }

        TEST_CASE("starting state rejects combat and StartMatch is idempotent") {
            FMatchWorld f;
            REQUIRE(f.GS);
            f.GS->SetMatchState(ELeonTournamentMatchState::Starting);
            auto* a = f.World->SpawnActor<ALeonTournamentCharacter>("A");
            auto* b = f.World->SpawnActor<ALeonTournamentCharacter>("B");
            auto* psa = f.World->SpawnActor<ALeonTournamentPlayerState>("PSA");
            auto* psb = f.World->SpawnActor<ALeonTournamentPlayerState>("PSB");
            psa->SetTeam(ELeonTournamentTeam::Team1);
            psb->SetTeam(ELeonTournamentTeam::Team2);
            auto* pca = f.World->SpawnActor<ALeonTournamentPlayerController>("PCA");
            auto* pcb = f.World->SpawnActor<ALeonTournamentPlayerController>("PCB");
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
            FLeonTournamentMatchConfig cfg;
            cfg.MaxPlayers = 4;
            cfg.StartCountdownSeconds = 0.0f;
            f2.GM->SetMatchConfig(cfg);
            f2.GM->StartMatch();
            size_t first = 0;
            for (const auto& actor : f2.World->GetAllActors()) {
                if (dynamic_cast<ALeonTournamentCharacter*>(actor.get()))
                    ++first;
            }
            f2.GM->StartMatch();
            size_t second = 0;
            for (const auto& actor : f2.World->GetAllActors()) {
                if (dynamic_cast<ALeonTournamentCharacter*>(actor.get()))
                    ++second;
            }
            CHECK(first == second);
            CHECK(first >= 4);
        }
    }

    TEST_SUITE("LeonTournament weapon") {

        TEST_CASE("fire rate ammo reload") {
            FMatchWorld f;
            f.GS->SetMatchState(ELeonTournamentMatchState::Playing);
            auto* ch = f.World->SpawnActor<ALeonTournamentCharacter>("Gunner");
            REQUIRE(ch->GetWeapon());
            auto* weap = ch->GetWeapon();
            const int32_t mag = weap->GetMagazineSize();
            CHECK(weap->GetCurrentAmmo() == mag);
            CHECK(weap->CanFire());
            CHECK(weap->ServerFire());
            CHECK(weap->GetCurrentAmmo() == mag - 1);
            CHECK_FALSE(weap->ServerFire());
            weap->Tick(0.2f);
            CHECK(weap->ServerFire());

            while (weap->GetCurrentAmmo() > 0) {
                weap->Tick(0.2f);
                weap->ServerFire();
            }
            CHECK(weap->GetCurrentAmmo() == 0);
            CHECK(weap->IsReloading());
            CHECK_FALSE(weap->CanFire());
            weap->Tick(2.1f);
            CHECK_FALSE(weap->IsReloading());
            CHECK(weap->GetCurrentAmmo() == mag);
        }

        TEST_CASE("laser is one-shot scoped hitscan") {
            FMatchWorld f;
            f.GS->SetMatchState(ELeonTournamentMatchState::Playing);
            auto* ch = f.World->SpawnActor<ALeonTournamentCharacter>("LaserGunner");
            REQUIRE(ch->GiveWeapon(ELeonTournamentWeaponId::Laser));
            auto* weap = ch->GetWeapon();
            REQUIRE(weap);
            CHECK(weap->GetWeaponId() == ELeonTournamentWeaponId::Laser);
            CHECK(weap->GetMagazineSize() == 1);
            CHECK(weap->CanAimDownSights());
            CHECK(weap->GetConfig().ScopeFOV == doctest::Approx(32.0f));
            CHECK(weap->GetConfig().FireMode == ELeonTournamentFireMode::Hitscan);
            CHECK(weap->ServerFire());
            CHECK(weap->GetCurrentAmmo() == 0);
            CHECK(weap->IsReloading());
        }

        TEST_CASE("grenade rocket and flame presets") {
            const auto grenade = LeonTournamentWeaponPreset(ELeonTournamentWeaponId::Grenade);
            CHECK(grenade.FireMode == ELeonTournamentFireMode::Projectile);
            CHECK(grenade.ProjectileGravityScale > 0.1f);
            CHECK(grenade.SplashRadius > 1.0f);
            CHECK(grenade.Knockback > 8.0f);

            const auto rocket = LeonTournamentWeaponPreset(ELeonTournamentWeaponId::Rocket);
            CHECK(rocket.FireMode == ELeonTournamentFireMode::Projectile);
            CHECK(rocket.ProjectileGravityScale == doctest::Approx(0.0f));

            const auto flame = LeonTournamentWeaponPreset(ELeonTournamentWeaponId::Flamethrower);
            CHECK(flame.FireMode == ELeonTournamentFireMode::Flame);
            CHECK(flame.FlameConeDeg > 1.0f);
        }

        TEST_CASE("projectile splash damages and launches") {
            FMatchWorld f;
            f.GS->SetMatchState(ELeonTournamentMatchState::Playing);
            auto* a = f.World->SpawnActor<ALeonTournamentCharacter>("BoomA");
            auto* b = f.World->SpawnActor<ALeonTournamentCharacter>("BoomB");
            a->SetActorLocation({0.0f, 2.0f, 0.0f});
            b->SetActorLocation({0.0f, 2.0f, 1.5f});
            REQUIRE(a->GiveWeapon(ELeonTournamentWeaponId::Rocket));
            auto* proj = f.World->SpawnActor<ALeonTournamentProjectile>("RocketProj");
            REQUIRE(proj);
            const auto cfg = LeonTournamentWeaponPreset(ELeonTournamentWeaponId::Rocket);
            proj->Launch(a, a->GetWeapon(), {0.0f, 0.0f, 1.0f}, cfg);
            FHitResult hit;
            hit.bBlockingHit = true;
            hit.Actor = b;
            hit.Location = b->GetActorLocation();
            hit.Normal = {0.0f, 1.0f, 0.0f};
            proj->NotifyHit(hit);
            CHECK(proj->HasExploded());
            REQUIRE(b->GetHealthComponent());
            CHECK(b->GetHealthComponent()->IsDead());
            CHECK(b->IsDeadFrozen());
        }

        TEST_CASE("weapon pickup grants and respawns after 15s") {
            FMatchWorld f;
            f.GS->SetMatchState(ELeonTournamentMatchState::Playing);
            auto* ch = f.World->SpawnActor<ALeonTournamentCharacter>("Picker");
            auto* pu = f.World->SpawnActor<ALeonTournamentWeaponPickup>("PU_Laser");
            REQUIRE(pu);
            pu->SetWeaponId(ELeonTournamentWeaponId::Laser);
            pu->SetRespawnDelay(15.0f);
            CHECK(pu->GetRespawnDelay() == doctest::Approx(15.0f));
            CHECK(pu->IsPickupActive());

            ch->SetActorLocation(pu->GetActorLocation());
            f.World->Tick(FTimestep(0.05f));
            CHECK(ch->HasWeapon(ELeonTournamentWeaponId::Laser));
            CHECK_FALSE(pu->IsPickupActive());

            ch->SetActorLocation(pu->GetActorLocation() + glm::vec3(0.0f, 0.0f, 20.0f));
            pu->Tick(14.0f);
            CHECK_FALSE(pu->IsPickupActive());
            pu->Tick(1.2f);
            CHECK(pu->IsPickupActive());
        }

        TEST_CASE("replicated ammo and reload reach the client pawn") {
            FMatchWorld f;
            f.GS->SetMatchState(ELeonTournamentMatchState::Playing);
            auto* src = f.World->SpawnActor<ALeonTournamentCharacter>("Src");
            auto* dst = f.World->SpawnActor<ALeonTournamentCharacter>("Dst");
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

    TEST_SUITE("LeonTournament net authority") {

        TEST_CASE("client cannot author health kills or team score") {
            RegisterLeonTournamentClasses();
            auto server = UWorld::Create("S");
            auto client = UWorld::Create("C");
            server->SetNetMode(ENetMode::ListenServer);
            client->SetNetMode(ENetMode::Client);
            ULoopbackNetDriver sd, cd;
            sd.SetWorld(server.get());
            cd.SetWorld(client.get());
            ULoopbackNetDriver::Pair(sd, cd);
            server->SetNetDriver(&sd);
            client->SetNetDriver(&cd);

            auto* gm = server->SpawnActor<ALeonTournamentGameMode>("GM");
            server->SetGameMode(gm);
            server->InitWorld();
            gm->HUDClass = "None";
            server->BeginPlay();
            auto* gs = gm->GetGameState();
            REQUIRE(gs);
            gs->SetMatchState(ELeonTournamentMatchState::Playing);
            gs->SetTeam1Kills(7);

            auto* ps = dynamic_cast<ALeonTournamentPlayerState*>(server->GetFirstPlayerController()->GetPlayerState());
            REQUIRE(ps);
            ps->AddKill();

            for (int i = 0; i < 4; ++i) {
                server->Tick(FTimestep(0.05f));
                client->Tick(FTimestep(0.05f));
            }

            auto* cgs = dynamic_cast<ALeonTournamentGameState*>(client->GetGameState());
            REQUIRE(cgs);
            CHECK(cgs->GetTeam1Kills() == 7);
            CHECK(cgs->GetMatchState() == ELeonTournamentMatchState::Playing);

            cgs->SetTeam1Kills(99);
            server->Tick(FTimestep(0.05f));
            client->Tick(FTimestep(0.05f));
            CHECK(dynamic_cast<ALeonTournamentGameState*>(client->GetGameState())->GetTeam1Kills() == 7);
        }
    }

    TEST_SUITE("LeonTournament ownership / scale / traces / bots") {

        TEST_CASE("GameInstance holds session data only") {
            ULeonTournamentGameInstance gi("GI");
            gi.SetSessionMode(ELeonTournamentSessionMode::LanClient);
            gi.SetJoinAddress("192.168.1.50");
            CHECK(gi.GetSessionMode() == ELeonTournamentSessionMode::LanClient);
            CHECK(gi.GetJoinAddress() == "192.168.1.50");
            CHECK(gi.GetLanPort() == 7777);
        }

        TEST_CASE("PlayerState stats survive respawn") {
            FMatchWorld f;
            f.GS->SetMatchState(ELeonTournamentMatchState::Playing);
            auto* ch = f.World->SpawnActor<ALeonTournamentCharacter>("P");
            auto* ps = f.World->SpawnActor<ALeonTournamentPlayerState>("PS");
            auto* pc = f.World->SpawnActor<ALeonTournamentPlayerController>("PC");
            ps->SetTeam(ELeonTournamentTeam::Team1);
            pc->SetPlayerState(ps);
            pc->Possess(ch);
            ps->AddKill();
            ps->AddDeath();
            ps->AddAssist();
            const FUUID oldGuid = ch->GetActorGuid();
            f.GM->RespawnCharacter(*ch);
            CHECK(ps->GetKills() == 1);
            CHECK(ps->GetDeaths() == 1);
            CHECK(ps->GetAssists() == 1);
            auto* spawned = pc->GetPawn<ALeonTournamentCharacter>();
            REQUIRE(spawned);
            CHECK(spawned->GetActorGuid() != oldGuid);
            CHECK(f.World->FindActorByGuid(oldGuid) == nullptr);
            CHECK(spawned->GetPlayerState() == ps);
            CHECK(spawned->GetHealthComponent()->GetHealth() == doctest::Approx(100.0f));
        }

        TEST_CASE("ammo lives on the weapon only") {
            FMatchWorld f;
            f.GS->SetMatchState(ELeonTournamentMatchState::Playing);
            auto* ch = f.World->SpawnActor<ALeonTournamentCharacter>("P");
            REQUIRE(ch->GetWeapon());
            CHECK(ch->GetWeapon()->GetCurrentAmmo() == ch->GetWeapon()->GetMagazineSize());
            CHECK(ch->GetWeapon()->ServerFire());
            CHECK(ch->GetWeapon()->GetCurrentAmmo() == ch->GetWeapon()->GetMagazineSize() - 1);
        }

        TEST_CASE("health lives on HealthComponent") {
            FMatchWorld f;
            f.GS->SetMatchState(ELeonTournamentMatchState::Playing);
            auto* a = f.World->SpawnActor<ALeonTournamentCharacter>("A");
            auto* b = f.World->SpawnActor<ALeonTournamentCharacter>("B");
            auto* psa = f.World->SpawnActor<ALeonTournamentPlayerState>("PSA");
            auto* psb = f.World->SpawnActor<ALeonTournamentPlayerState>("PSB");
            psa->SetTeam(ELeonTournamentTeam::Team1);
            psb->SetTeam(ELeonTournamentTeam::Team2);
            auto* pca = f.World->SpawnActor<ALeonTournamentPlayerController>("PCA");
            auto* pcb = f.World->SpawnActor<ALeonTournamentPlayerController>("PCB");
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
            auto* a = f.World->SpawnActor<ALeonTournamentCharacter>("A");
            auto* b = f.World->SpawnActor<ALeonTournamentCharacter>("B");
            auto* psa = f.World->SpawnActor<ALeonTournamentPlayerState>("PSA");
            auto* psb = f.World->SpawnActor<ALeonTournamentPlayerState>("PSB");
            psa->SetTeam(ELeonTournamentTeam::Team1);
            psb->SetTeam(ELeonTournamentTeam::Team2);
            auto* pca = f.World->SpawnActor<ALeonTournamentPlayerController>("PCA");
            auto* pcb = f.World->SpawnActor<ALeonTournamentPlayerController>("PCB");
            pca->SetPlayerState(psa);
            pcb->SetPlayerState(psb);
            pca->Possess(a);
            pcb->Possess(b);
            FDamageInfo info;
            info.DamageAmount = 25.0f;
            info.Instigator = a;
            f.GS->SetMatchState(ELeonTournamentMatchState::Lobby);
            CHECK_FALSE(f.GM->ApplyAuthoritativeDamage(*a, *b, info));
            f.GS->SetMatchState(ELeonTournamentMatchState::Finished);
            CHECK_FALSE(f.GM->ApplyAuthoritativeDamage(*a, *b, info));
        }

        TEST_CASE("character capsule matches world-unit human height") {
            FMatchWorld f;
            auto* ch = f.World->SpawnActor<ALeonTournamentCharacter>("Human");
            CHECK(ch->GetEyeHeight() == doctest::Approx(FWorldUnits::ExpectedEyeHeight));
            CHECK(ch->GetCapsuleHeight() >= FWorldUnits::ExpectedHumanHeightMin);
            CHECK(ch->GetCapsuleHeight() <= FWorldUnits::ExpectedHumanHeightMax);
            CHECK(3.0f > ch->GetCapsuleHeight());
            CHECK(1.4f < ch->GetEyeHeight());
        }

        TEST_CASE("spawn validation does not crash a possessed pawn") {
            FMatchWorld f;
            f.GM->BuildArena();
            f.GS->SetMatchState(ELeonTournamentMatchState::Playing);
            auto* ch = f.World->SpawnActor<ALeonTournamentCharacter>("Spawned");
            auto* ps = f.World->SpawnActor<ALeonTournamentPlayerState>("PS");
            auto* pc = f.World->SpawnActor<ALeonTournamentPlayerController>("PC");
            ps->SetTeam(ELeonTournamentTeam::Team1);
            pc->SetPlayerState(ps);
            ch->SetActorLocation(f.GM->GetTeamSpawnLocation(ELeonTournamentTeam::Team1));
            pc->Possess(ch);
            CHECK(ch->GetController() == pc);
            CHECK(ch->GetWeapon() != nullptr);
            CHECK(ch->GetHealthComponent());
            CHECK_FALSE(ch->GetHealthComponent()->IsDead());
        }

        TEST_CASE("wall occludes hitscan between enemies") {
            FMatchWorld f;
            f.GS->SetMatchState(ELeonTournamentMatchState::Playing);
            auto* wall = f.World->SpawnActor<AActor>("Wall");
            wall->SetActorLocation({0.0f, 1.0f, 5.0f});
            wall->AddComponent<FBoxCollisionComponent>(glm::vec3(-2.0f, -2.0f, -0.3f), glm::vec3(2.0f, 2.0f, 0.3f),
                                                       true, ECollisionChannel::WorldStatic);
            auto* a = f.World->SpawnActor<ALeonTournamentCharacter>("A");
            auto* b = f.World->SpawnActor<ALeonTournamentCharacter>("B");
            auto* psa = f.World->SpawnActor<ALeonTournamentPlayerState>("PSA");
            auto* psb = f.World->SpawnActor<ALeonTournamentPlayerState>("PSB");
            psa->SetTeam(ELeonTournamentTeam::Team1);
            psb->SetTeam(ELeonTournamentTeam::Team2);
            auto* pca = f.World->SpawnActor<ALeonTournamentPlayerController>("PCA");
            auto* pcb = f.World->SpawnActor<ALeonTournamentPlayerController>("PCB");
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
            auto* pawn = f.World->SpawnActor<ALeonTournamentCharacter>("Pawn");
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
            auto* pc = f.World->SpawnActor<ALeonTournamentPlayerController>("PC");
            CHECK_FALSE(pc->IsHitMarkerActive());
            pc->NotifyConfirmedHit(false);
            CHECK(pc->IsHitMarkerActive());
            CHECK_FALSE(pc->IsKillConfirmActive());
            pc->NotifyConfirmedHit(true);
            CHECK(pc->IsKillConfirmActive());
            pc->Tick(0.25f);
            CHECK_FALSE(pc->IsHitMarkerActive());
            pc->NotifyTookDamage();
            CHECK(pc->IsDamageFlashActive());
            pc->Tick(0.3f);
            CHECK_FALSE(pc->IsDamageFlashActive());
        }

        TEST_CASE("bot acquires an enemy and leaves Search") {
            FMatchWorld f;
            f.GS->SetMatchState(ELeonTournamentMatchState::Playing);
            auto* botPawn = f.World->SpawnActor<ALeonTournamentCharacter>("BotPawn");
            auto* enemy = f.World->SpawnActor<ALeonTournamentCharacter>("Enemy");
            auto* psBot = f.World->SpawnActor<ALeonTournamentPlayerState>("PSBot");
            auto* psEnemy = f.World->SpawnActor<ALeonTournamentPlayerState>("PSEnemy");
            psBot->SetTeam(ELeonTournamentTeam::Team1);
            psBot->SetIsBot(true);
            psEnemy->SetTeam(ELeonTournamentTeam::Team2);
            auto* bot = f.World->SpawnActor<ALeonTournamentBotController>("BotPC");
            auto* enemyPc = f.World->SpawnActor<ALeonTournamentPlayerController>("EnemyPC");
            bot->SetPlayerState(psBot);
            enemyPc->SetPlayerState(psEnemy);
            botPawn->SetActorLocation({0.0f, 1.7f, 0.0f});
            enemy->SetActorLocation({4.0f, 1.7f, 0.0f});
            bot->Possess(botPawn);
            enemyPc->Possess(enemy);
            for (int i = 0; i < 20; ++i)
                bot->Tick(0.05f);
            CHECK(bot->GetBotState() != ELeonTournamentBotState::Idle);
            CHECK(bot->GetCurrentTarget() == enemy);
            CHECK(bot->HasLineOfSight(*enemy));
        }

        TEST_CASE("GameState countdown replicates with match state") {
            FMatchWorld f;
            f.GS->SetMatchState(ELeonTournamentMatchState::Starting);
            f.GS->SetCountdownRemaining(1.5f);
            std::vector<uint8_t> blob;
            f.GS->SerializeReplication(blob);
            auto* other = f.World->SpawnActor<ALeonTournamentGameState>("GS2");
            other->DeserializeReplication(blob.data(), blob.size());
            CHECK(other->GetMatchState() == ELeonTournamentMatchState::Starting);
            CHECK(other->GetCountdownRemaining() == doctest::Approx(1.5f));
        }

        TEST_CASE("camera aim ray uses look when not locally controlled") {
            FMatchWorld f;
            auto* ch = f.World->SpawnActor<ALeonTournamentCharacter>("Aim");
            ch->SetControlYaw(90.0f);
            ch->SetControlPitch(0.0f);
            glm::vec3 origin, dir;
            ch->GetAimRay(origin, dir);
            CHECK(dir.z == doctest::Approx(1.0f).epsilon(0.02f));
        }
    }

} // namespace Leon
