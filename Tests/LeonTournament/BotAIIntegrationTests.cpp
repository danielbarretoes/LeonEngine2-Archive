#include <doctest/doctest.h>

#include "Core/FTimestep.hpp"
#include "Engine/UWorld.hpp"
#include "Gameplay/UClassRegistry.hpp"
#include "Gameplay/ANavMeshBoundsVolume.hpp"
#include "Gameplay/UPrimitiveComponent.hpp"
#include "Gameplay/AAIController.hpp"
#include "Gameplay/ACharacter.hpp"
#include "Gameplay/UCharacterMovementComponent.hpp"
#include "AI/UNavigationSystem.hpp"
#include "AI/FNavTypes.hpp"
#include "ALeonTournamentGameMode.hpp"
#include "ALeonTournamentGameState.hpp"
#include "ALeonTournamentCharacter.hpp"
#include "ALeonTournamentPlayerController.hpp"
#include "ALeonTournamentPlayerState.hpp"
#include "ALeonTournamentHUD.hpp"
#include "ALeonTournamentBotController.hpp"
#include "ALeonTournamentWeapon.hpp"
#include "LeonTournamentTestSetup.hpp"
#include "Engine/Components.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>
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
                World = UWorld::Create("BotAI");
                GM = World->SpawnActor<ALeonTournamentGameMode>("GM");
                World->SetGameMode(GM);
                World->InitWorld();
                GM->HUDClass = "None";
                World->BeginPlay();
                GS = GM->GetGameState();
            }

            void TickSeconds(float InSeconds, float InDt = 1.0f / 30.0f) {
                int32_t steps = std::max(1, static_cast<int32_t>(InSeconds / InDt));
                for (int32_t i = 0; i < steps; ++i)
                    World->Tick(FTimestep(InDt));
            }
        };

        AActor* SpawnStaticBox(UWorld& InWorld, const std::string& InName, const glm::vec3& InLocation,
                               const glm::vec3& InScale) {
            AActor* actor = InWorld.SpawnActor<AActor>(InName);
            actor->SetActorLocation(InLocation);
            actor->SetActorScale(InScale);
            auto box = actor->AddActorComponent<UBoxComponent>("Box");
            box->SetBoxExtent(glm::vec3(0.5f));
            box->SetCollisionObjectType(ECollisionChannel::WorldStatic);
            return actor;
        }

        ALeonTournamentBotController* FirstBot(UWorld& InWorld) {
            for (AAIController* ai : InWorld.GetAIControllers()) {
                if (auto* bot = dynamic_cast<ALeonTournamentBotController*>(ai))
                    return bot;
            }
            return nullptr;
        }
    } // namespace

    TEST_SUITE("BotSpawnIntegrationTest") {
        TEST_CASE("three bots spawn with controller pawn blackboard and BT") {
            FMatchWorld f;
            FLeonTournamentMatchConfig cfg;
            cfg.MaxPlayers = 4;
            cfg.StartCountdownSeconds = 0.0f;
            f.GM->SetMatchConfig(cfg);
            f.GM->StartMatch();

            auto* nav = f.World->GetNavigationSystem();
            REQUIRE(nav);
            CHECK(nav->IsBuilt());
            CHECK(nav->GetWalkableCount() > 200);

            const auto& ais = f.World->GetAIControllers();
            CHECK(ais.size() >= 3);
            int32_t ready = 0;
            for (AAIController* ai : ais) {
                auto* bot = dynamic_cast<ALeonTournamentBotController*>(ai);
                REQUIRE(bot);
                CHECK(bot->GetPawn<ALeonTournamentCharacter>() != nullptr);
                CHECK(bot->GetBlackboardComponent() != nullptr);
                CHECK(bot->IsBehaviorTreeRunning());
                ++ready;
            }
            CHECK(ready >= 3);
        }
    }

    TEST_SUITE("BotPathTest") {
        TEST_CASE("FindPath around a wall is valid and longer than the straight line") {
            auto world = UWorld::Create("NavWall");
            world->BeginPlay();
            SpawnStaticBox(*world, "Floor", {0.0f, -0.25f, 0.0f}, {40.0f, 0.5f, 40.0f});
            SpawnStaticBox(*world, "Wall", {0.0f, 1.5f, 0.0f}, {1.2f, 3.0f, 18.0f});
            auto* bounds = world->SpawnActor<ANavMeshBoundsVolume>("NavBounds");
            bounds->SetActorLocation({0.0f, 1.0f, 0.0f});
            bounds->SetActorScale({36.0f, 2.0f, 36.0f});
            world->RebuildNavigation();

            auto* nav = world->GetNavigationSystem();
            REQUIRE(nav);
            REQUIRE(nav->IsBuilt());
            CHECK(nav->GetWalkableCount() > 50);

            const FNavPath path = nav->FindPath({-10.0f, 1.7f, 0.0f}, {10.0f, 1.7f, 0.0f});
            CHECK(path.Status != ENavPathStatus::Invalid);
            CHECK(path.IsValid());
            CHECK(path.Points.size() >= 3);
            CHECK(path.Length > 22.0f);
        }
    }

    TEST_SUITE("BotNavigationIntegrationTest") {
        TEST_CASE("bot physically leaves spawn after StartMatch") {
            FMatchWorld f;
            FLeonTournamentMatchConfig cfg;
            cfg.MaxPlayers = 4;
            cfg.StartCountdownSeconds = 0.0f;
            f.GM->SetMatchConfig(cfg);
            f.GM->StartMatch();
            f.TickSeconds(0.05f);

            auto* bot = FirstBot(*f.World);
            REQUIRE(bot);
            auto* pawn = bot->GetPawn<ALeonTournamentCharacter>();
            REQUIRE(pawn);
            const glm::vec3 start = pawn->GetActorLocation();
            f.TickSeconds(3.0f);
            glm::vec3 delta = pawn->GetActorLocation() - start;
            delta.y = 0.0f;
            CHECK(glm::length(delta) > 1.5f);
        }
    }

    TEST_SUITE("BotPerceptionTest") {
        TEST_CASE("visible enemy sets TargetActor and HasLOS") {
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
            f.World->AddAIController(bot);
            botPawn->SetActorLocation({0.0f, 1.7f, 0.0f});
            enemy->SetActorLocation({0.0f, 1.7f, -8.0f});
            bot->Possess(botPawn);
            enemyPc->Possess(enemy);
            for (int i = 0; i < 12; ++i)
                bot->Tick(0.05f);
            CHECK(bot->GetCurrentTarget() == enemy);
            CHECK(bot->HasLineOfSight(*enemy));
            auto board = bot->GetBlackboardComponent();
            REQUIRE(board);
            CHECK(board->GetValueAsBool("HasTarget"));
            CHECK(board->GetValueAsBool("HasLineOfSight"));
        }

        TEST_CASE("wall blocks LOS") {
            FMatchWorld f;
            f.GS->SetMatchState(ELeonTournamentMatchState::Playing);
            SpawnStaticBox(*f.World, "Wall", {0.0f, 1.5f, -4.0f}, {6.0f, 3.0f, 0.8f});
            auto* botPawn = f.World->SpawnActor<ALeonTournamentCharacter>("BotPawn");
            auto* enemy = f.World->SpawnActor<ALeonTournamentCharacter>("Enemy");
            auto* psBot = f.World->SpawnActor<ALeonTournamentPlayerState>("PSBot");
            auto* psEnemy = f.World->SpawnActor<ALeonTournamentPlayerState>("PSEnemy");
            psBot->SetTeam(ELeonTournamentTeam::Team1);
            psEnemy->SetTeam(ELeonTournamentTeam::Team2);
            auto* bot = f.World->SpawnActor<ALeonTournamentBotController>("BotPC");
            auto* enemyPc = f.World->SpawnActor<ALeonTournamentPlayerController>("EnemyPC");
            bot->SetPlayerState(psBot);
            enemyPc->SetPlayerState(psEnemy);
            botPawn->SetActorLocation({0.0f, 1.7f, 0.0f});
            enemy->SetActorLocation({0.0f, 1.7f, -8.0f});
            bot->Possess(botPawn);
            enemyPc->Possess(enemy);
            CHECK_FALSE(bot->HasLineOfSight(*enemy));
        }
    }

    TEST_SUITE("BotCombatIntegrationTest") {
        TEST_CASE("bot with LOS fires and damages the enemy") {
            FMatchWorld f;
            FLeonTournamentMatchConfig cfg;
            cfg.MaxPlayers = 2;
            cfg.StartCountdownSeconds = 0.0f;
            f.GM->SetMatchConfig(cfg);
            f.GM->StartMatch();
            f.TickSeconds(0.05f);
            REQUIRE(f.GS->GetMatchState() == ELeonTournamentMatchState::Playing);

            auto* bot = FirstBot(*f.World);
            auto* humanPc = f.World->GetFirstPlayerController();
            REQUIRE(bot);
            REQUIRE(humanPc);
            auto* botPawn = bot->GetPawn<ALeonTournamentCharacter>();
            auto* enemy = humanPc->GetPawn<ALeonTournamentCharacter>();
            REQUIRE(botPawn);
            REQUIRE(enemy);
            botPawn->SetActorLocation({-16.0f, 1.7f, 0.0f});
            enemy->SetActorLocation({-16.0f, 1.7f, -10.0f});
            botPawn->SetControlRotation({0.0f, -90.0f, 0.0f});
            const float startHp = enemy->GetHealthComponent()->GetHealth();
            const int32_t startAmmo = botPawn->GetWeapon() ? botPawn->GetWeapon()->GetCurrentAmmo() : 0;
            f.TickSeconds(2.5f);
            const float endHp = enemy->GetHealthComponent()->GetHealth();
            const int32_t endAmmo = botPawn->GetWeapon() ? botPawn->GetWeapon()->GetCurrentAmmo() : 0;
            CHECK(endAmmo < startAmmo);
            CHECK(endHp < startHp);
            CHECK((bot->GetCurrentTarget() == enemy || enemy->GetHealthComponent()->IsDead()));
        }
    }

    TEST_SUITE("BotDeathRespawnTest") {
        TEST_CASE("dead bot stops combat then respawns with BT and can move") {
            FMatchWorld f;
            FLeonTournamentMatchConfig cfg;
            cfg.MaxPlayers = 2;
            cfg.StartCountdownSeconds = 0.0f;
            cfg.RespawnDelaySeconds = 0.4f;
            f.GM->SetMatchConfig(cfg);
            f.GM->StartMatch();
            f.TickSeconds(0.05f);

            auto* bot = FirstBot(*f.World);
            REQUIRE(bot);
            auto* pawn = bot->GetPawn<ALeonTournamentCharacter>();
            REQUIRE(pawn);
            pawn->GetHealthComponent()->ApplyDamage(500.0f);
            bot->Tick(0.05f);
            CHECK(bot->GetBotState() == ELeonTournamentBotState::Dead);
            CHECK(pawn->GetHealthComponent()->IsDead());
            ALeonTournamentPlayerState* ps = dynamic_cast<ALeonTournamentPlayerState*>(bot->GetPlayerState());
            REQUIRE(ps);
            const FUUID oldGuid = pawn->GetActorGuid();

            f.TickSeconds(1.2f);
            auto* spawned = bot->GetPawn<ALeonTournamentCharacter>();
            REQUIRE(spawned);
            CHECK(spawned->GetActorGuid() != oldGuid);
            CHECK(f.World->FindActorByGuid(oldGuid) == nullptr);
            CHECK(bot->GetPlayerState() == ps);
            CHECK_FALSE(spawned->GetHealthComponent()->IsDead());
            CHECK(bot->IsBehaviorTreeRunning());
            CHECK(bot->GetBotState() != ELeonTournamentBotState::Dead);

            const glm::vec3 start = spawned->GetActorLocation();
            f.TickSeconds(2.5f);
            glm::vec3 delta = spawned->GetActorLocation() - start;
            delta.y = 0.0f;
            CHECK(glm::length(delta) > 0.8f);
        }
    }

    TEST_SUITE("BotFullMatchIntegrationTest") {
        TEST_CASE("2v2 ticks prove bots play: move acquire shoot damage") {
            FMatchWorld f;
            FLeonTournamentMatchConfig cfg;
            cfg.MaxPlayers = 4;
            cfg.StartCountdownSeconds = 0.0f;
            cfg.MatchDurationSeconds = 120.0f;
            f.GM->SetMatchConfig(cfg);
            f.GM->StartMatch();
            f.TickSeconds(0.05f);
            CHECK(f.GS->GetMatchState() == ELeonTournamentMatchState::Playing);
            CHECK(f.World->GetAIControllers().size() >= 3);

            std::unordered_map<ALeonTournamentBotController*, glm::vec3> starts;
            int32_t startAmmoSum = 0;
            for (AAIController* ai : f.World->GetAIControllers()) {
                auto* bot = dynamic_cast<ALeonTournamentBotController*>(ai);
                auto* pawn = bot ? bot->GetPawn<ALeonTournamentCharacter>() : nullptr;
                if (!bot || !pawn)
                    continue;
                starts[bot] = pawn->GetActorLocation();
                if (pawn->GetWeapon())
                    startAmmoSum += pawn->GetWeapon()->GetCurrentAmmo();
            }

            float startHpSum = 0.0f;
            for (const auto& actor : f.World->GetAllActors()) {
                auto* ch = dynamic_cast<ALeonTournamentCharacter*>(actor.get());
                if (ch && ch->GetHealthComponent())
                    startHpSum += ch->GetHealthComponent()->GetHealth();
            }
            const int32_t startScore = f.GS->GetTeam1Kills() + f.GS->GetTeam2Kills();

            f.TickSeconds(12.0f, 1.0f / 20.0f);

            int32_t moved = 0;
            int32_t acquired = 0;
            int32_t endAmmoSum = 0;
            for (auto& [bot, start] : starts) {
                auto* pawn = bot->GetPawn<ALeonTournamentCharacter>();
                if (!pawn)
                    continue;
                glm::vec3 delta = pawn->GetActorLocation() - start;
                delta.y = 0.0f;
                if (glm::length(delta) > 2.0f)
                    ++moved;
                if (bot->GetCurrentTarget() || bot->GetBotState() == ELeonTournamentBotState::Combat ||
                    bot->GetBotState() == ELeonTournamentBotState::Fire || bot->GetBotState() == ELeonTournamentBotState::Search)
                    ++acquired;
                if (pawn->GetWeapon())
                    endAmmoSum += pawn->GetWeapon()->GetCurrentAmmo();
            }

            float endHpSum = 0.0f;
            int32_t deadOrRespawned = 0;
            for (const auto& actor : f.World->GetAllActors()) {
                auto* ch = dynamic_cast<ALeonTournamentCharacter*>(actor.get());
                if (!ch || !ch->GetHealthComponent())
                    continue;
                endHpSum += ch->GetHealthComponent()->GetHealth();
                if (ch->GetHealthComponent()->IsDead() || ch->GetHealthComponent()->GetHealth() < 99.0f)
                    ++deadOrRespawned;
            }
            const int32_t endScore = f.GS->GetTeam1Kills() + f.GS->GetTeam2Kills();

            CHECK(moved * 5 >= static_cast<int32_t>(starts.size()) * 4);
            CHECK(acquired >= 1);
            CHECK(endAmmoSum < startAmmoSum);
            CHECK(endHpSum < startHpSum);
            CHECK((deadOrRespawned >= 1 || endScore > startScore));
        }
    }

} // namespace Leon
