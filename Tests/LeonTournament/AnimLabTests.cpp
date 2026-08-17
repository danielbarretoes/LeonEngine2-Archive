#include <doctest/doctest.h>

#include "Core/FTimestep.hpp"
#include "Engine/UWorld.hpp"
#include "Gameplay/APlayerStart.hpp"
#include "Gameplay/UClassRegistry.hpp"
#include "Gameplay/UCombatComponent.hpp"
#include "Gameplay/UHealthComponent.hpp"
#include "ALeonTournamentAnimLabGameMode.hpp"
#include "ALeonTournamentCharacter.hpp"
#include "ALeonTournamentDummy.hpp"
#include "ALeonTournamentGameMode.hpp"
#include "ALeonTournamentGameState.hpp"
#include "ALeonTournamentHUD.hpp"
#include "ALeonTournamentPlayerController.hpp"
#include "ALeonTournamentPlayerState.hpp"
#include "ALeonTournamentBotController.hpp"
#include "ALeonTournamentWeapon.hpp"
#include "ULeonTournamentAnimInstance.hpp"
#include "Assets/UAssetManager.hpp"
#include "LeonTournamentTestSetup.hpp"

#include <cmath>
#include <glm/glm.hpp>

namespace Leon {

    namespace {
        void RegisterAnimLabClasses() {
            Test::BindLeonTournamentProject();
            auto& r = UClassRegistry::Get();
            r.RegisterClass<ALeonTournamentGameMode>("ALeonTournamentGameMode");
            r.RegisterClass<ALeonTournamentAnimLabGameMode>("ALeonTournamentAnimLabGameMode");
            r.RegisterClass<ALeonTournamentGameState>("ALeonTournamentGameState");
            r.RegisterClass<ALeonTournamentCharacter>("ALeonTournamentCharacter");
            r.RegisterClass<ALeonTournamentDummy>("ALeonTournamentDummy");
            r.RegisterClass<ALeonTournamentPlayerState>("ALeonTournamentPlayerState");
            r.RegisterClass<ALeonTournamentPlayerController>("ALeonTournamentPlayerController");
            r.RegisterClass<ALeonTournamentHUD>("ALeonTournamentHUD");
            r.RegisterClass<ALeonTournamentBotController>("ALeonTournamentBotController");
            r.RegisterClass<ALeonTournamentRifle>("ALeonTournamentRifle");
        }

        struct FAnimLabWorld {
            TRef<UWorld> World;
            ALeonTournamentAnimLabGameMode* GM = nullptr;
            APlayerStart* PlayerStart = nullptr;

            FAnimLabWorld() {
                RegisterAnimLabClasses();
                World = UWorld::Create("AnimLabWorld");
                PlayerStart = World->SpawnActor<APlayerStart>("PlayerStart");
                PlayerStart->SetActorLocation({-4.0f, 2.0f, 0.0f});
                PlayerStart->SetPlayerStartTag("Player");
                PlayerStart->SetTeamIndex(1);
                auto* dummyStart = World->SpawnActor<APlayerStart>("DummyStart");
                dummyStart->SetActorLocation({5.0f, 2.0f, 0.0f});
                dummyStart->SetPlayerStartTag("Dummy");
                dummyStart->SetTeamIndex(2);
                GM = World->SpawnActor<ALeonTournamentAnimLabGameMode>("GM");
                World->SetGameMode(GM);
                World->InitWorld();
                GM->HUDClass = "None";
                World->BeginPlay();
            }
        };
    } // namespace

    TEST_SUITE("AnimLabThirdPerson") {

        TEST_CASE("player spawns at map PlayerStart in third person") {
            FAnimLabWorld f;
            auto* pc = f.World->GetFirstPlayerController();
            REQUIRE(pc);
            auto* ch = pc->GetPawn<ALeonTournamentCharacter>();
            REQUIRE(ch);
            CHECK(ch->IsThirdPerson());
            CHECK(glm::length(ch->GetActorLocation() - f.PlayerStart->GetActorLocation()) < 0.05f);
        }

        TEST_CASE("dummy has health and combat") {
            FAnimLabWorld f;
            auto* dummy = f.GM->GetDummy();
            REQUIRE(dummy);
            REQUIRE(dummy->GetHealthComponent());
            REQUIRE(dummy->GetCombatComponent());
            CHECK_FALSE(dummy->GetHealthComponent()->IsDead());
            CHECK(dummy->GetCombatComponent()->CanAttack());
            CHECK(dummy->ShouldSpawnWeapon() == false);
            CHECK(dummy->GetWeapon() == nullptr);
            CHECK(dummy->IsThirdPerson());
        }

        TEST_CASE("rifle damage kills dummy and keeps the mesh for ragdoll") {
            FAnimLabWorld f;
            auto* dummy = f.GM->GetDummy();
            REQUIRE(dummy);
            dummy->GetHealthComponent()->ApplyDamage(500.0f);
            CHECK(dummy->GetHealthComponent()->IsDead());
            CHECK(dummy->IsDeadFrozen());
            if (auto mesh = dummy->GetMesh())
                CHECK_FALSE(mesh->IsHiddenInGame());
            if (auto cap = dummy->GetCapsuleComponent())
                CHECK(cap->IsSimulatingPhysics());
        }

        TEST_CASE("dummy Attack applies health damage") {
            FAnimLabWorld f;
            auto* pc = f.World->GetFirstPlayerController();
            auto* player = pc->GetPawn<ALeonTournamentCharacter>();
            auto* dummy = f.GM->GetDummy();
            REQUIRE(player);
            REQUIRE(dummy);
            player->SetActorLocation(dummy->GetActorLocation() + glm::vec3(0.8f, 0.0f, 0.0f));
            const float before = player->GetHealthComponent()->GetHealth();
            REQUIRE(dummy->GetCombatComponent()->Attack());
            CHECK(player->GetHealthComponent()->GetHealth() < before);
        }

        TEST_CASE("dummy respawns after death delay") {
            FAnimLabWorld f;
            auto* dummy = f.GM->GetDummy();
            REQUIRE(dummy);
            const glm::vec3 spawnLoc = dummy->GetActorLocation();
            dummy->GetHealthComponent()->ApplyDamage(500.0f);
            REQUIRE(dummy->IsDeadFrozen());
            for (int i = 0; i < 90; ++i)
                f.World->Tick(FTimestep(0.1f));
            CHECK_FALSE(dummy->IsDeadFrozen());
            CHECK_FALSE(dummy->GetHealthComponent()->IsDead());
            CHECK(glm::length(dummy->GetActorLocation() - spawnLoc) < 1.5f);
        }

        TEST_CASE("lab camera preference can switch to first person") {
            FAnimLabWorld f;
            auto* pc = f.World->GetFirstPlayerController();
            auto* ch = pc->GetPawn<ALeonTournamentCharacter>();
            REQUIRE(ch);
            CHECK(ch->IsThirdPerson());
            f.GM->SetPreferThirdPerson(false);
            f.GM->RestartPlayer(pc);
            ch = pc->GetPawn<ALeonTournamentCharacter>();
            REQUIRE(ch);
            CHECK_FALSE(ch->IsThirdPerson());
            CHECK_FALSE(f.GM->PrefersThirdPerson());
        }
    }

    TEST_SUITE("JumpCycleGraph") {
        TEST_CASE("graph has JumpStart Falling Landing Death") {
            Test::BindLeonTournamentProject();
            auto skel = UAssetManager::GetSkeleton("/Game/Skeletons/YBot.lskeleton");
            REQUIRE(skel);
            ULeonTournamentAnimInstance anim;
            anim.Initialize(skel);
            bool hasStart = false, hasFall = false, hasLand = false, hasDeath = false, hasLoco = false;
            for (const auto& state : anim.GetStateMachine().GetStates()) {
                hasLoco = hasLoco || state.Name == "Locomotion";
                hasStart = hasStart || state.Name == "JumpStart";
                hasFall = hasFall || state.Name == "Falling";
                hasLand = hasLand || state.Name == "Landing";
                hasDeath = hasDeath || state.Name == "Death";
            }
            CHECK(hasLoco);
            CHECK(hasStart);
            CHECK(hasFall);
            CHECK(hasLand);
            CHECK(hasDeath);
        }
    }

    TEST_SUITE("ArenaPlayerStarts") {
        TEST_CASE("map-placed PlayerStarts are preferred") {
            RegisterAnimLabClasses();
            auto world = UWorld::Create("SpawnPref");
            auto* gm = world->SpawnActor<ALeonTournamentGameMode>("GM");
            world->SetGameMode(gm);
            world->InitWorld();
            gm->HUDClass = "None";
            world->BeginPlay();
            auto* start = world->SpawnActor<APlayerStart>("MapStart");
            start->SetActorLocation({7.0f, 2.0f, 3.0f});
            start->SetTeamIndex(1);
            const glm::vec3 loc = gm->GetTeamSpawnLocation(ELeonTournamentTeam::Team1);
            CHECK(glm::length(loc - glm::vec3(7.0f, 2.0f, 3.0f)) < 0.01f);
        }

        TEST_CASE("arena builds tall maze walls and extra waypoints") {
            RegisterAnimLabClasses();
            auto world = UWorld::Create("ArenaMaze");
            auto* gm = world->SpawnActor<ALeonTournamentGameMode>("GM");
            world->SetGameMode(gm);
            world->InitWorld();
            gm->HUDClass = "None";
            world->BeginPlay();
            gm->BuildArena();
            CHECK(gm->GetWaypoints().size() >= 16);
            AActor* wallN = world->FindActorByName("WallN");
            REQUIRE(wallN);
            CHECK(wallN->GetActorScale().y >= 4.9f);
            REQUIRE(world->FindActorByName("MazeW_A"));
        }
    }

} // namespace Leon
