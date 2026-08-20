#include <doctest/doctest.h>

#include "Core/FTimestep.hpp"
#include "Engine/ULoopbackNetDriver.hpp"
#include "Engine/UWorld.hpp"
#include "Gameplay/UClassRegistry.hpp"
#include "ALeonTournamentGameMode.hpp"
#include "ALeonTournamentGameState.hpp"
#include "ALeonTournamentCharacter.hpp"
#include "ALeonTournamentPlayerController.hpp"
#include "ALeonTournamentPlayerState.hpp"
#include "ALeonTournamentHUD.hpp"
#include "ALeonTournamentBotController.hpp"
#include "ALeonTournamentWeapon.hpp"
#include "Gameplay/AAIController.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Gameplay/APlayerState.hpp"
#include "Engine/ENetTypes.hpp"
#include "LeonTournamentTestSetup.hpp"

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
                World = UWorld::Create("LifecycleMatch");
                GM = World->SpawnActor<ALeonTournamentGameMode>("GM");
                World->SetGameMode(GM);
                World->InitWorld();
                GM->HUDClass = "None";
                World->BeginPlay();
                GS = GM->GetGameState();
            }

            void TickSeconds(float InSeconds) {
                const float dt = 1.0f / 20.0f;
                float acc = 0.0f;
                while (acc < InSeconds) {
                    World->Tick(FTimestep(dt));
                    acc += dt;
                }
            }
        };

        int32_t CountLiveCharacters(UWorld* InWorld) {
            int32_t n = 0;
            for (const auto& actor : InWorld->GetAllActors()) {
                auto* ch = dynamic_cast<ALeonTournamentCharacter*>(actor.get());
                if (ch && !ch->IsPendingKill())
                    ++n;
            }
            return n;
        }

        ALeonTournamentBotController* FirstBot(UWorld& InWorld) {
            for (AAIController* ai : InWorld.GetAIControllers()) {
                if (auto* bot = dynamic_cast<ALeonTournamentBotController*>(ai))
                    return bot;
            }
            return nullptr;
        }
    } // namespace

    TEST_SUITE("LeonTournament lifecycle") {

        TEST_CASE("death replaces pawn and keeps controller plus PlayerState") {
            FMatchWorld f;
            FLeonTournamentMatchConfig cfg;
            cfg.MaxPlayers = 2;
            cfg.StartCountdownSeconds = 0.0f;
            cfg.RespawnDelaySeconds = 0.2f;
            f.GM->SetMatchConfig(cfg);
            f.GM->StartMatch();
            f.TickSeconds(0.05f);

            APlayerController* pc = f.World->GetFirstPlayerController();
            REQUIRE(pc);
            auto* ps = dynamic_cast<ALeonTournamentPlayerState*>(pc->GetPlayerState());
            REQUIRE(ps);
            auto* oldPawn = pc->GetPawn<ALeonTournamentCharacter>();
            REQUIRE(oldPawn);
            const FUUID oldGuid = oldPawn->GetActorGuid();
            oldPawn->GetHealthComponent()->ApplyDamage(500.0f);
            CHECK(oldPawn->GetHealthComponent()->IsDead());
            CHECK(pc->GetPawn() == oldPawn);

            f.TickSeconds(0.6f);
            auto* spawned = pc->GetPawn<ALeonTournamentCharacter>();
            REQUIRE(spawned);
            CHECK(spawned->GetActorGuid() != oldGuid);
            CHECK(f.World->FindActorByGuid(oldGuid) == nullptr);
            CHECK(pc->GetPlayerState() == ps);
            CHECK(spawned->GetPlayerState() == ps);
            CHECK_FALSE(spawned->GetHealthComponent()->IsDead());
            CHECK(f.GS == f.World->GetGameState());
        }

        TEST_CASE("match then finished then match keeps PlayerState identity") {
            FMatchWorld f;
            FLeonTournamentMatchConfig cfg;
            cfg.MaxPlayers = 4;
            cfg.StartCountdownSeconds = 0.0f;
            f.GM->SetMatchConfig(cfg);
            f.GM->StartMatch();
            f.TickSeconds(0.05f);
            APlayerController* pc = f.World->GetFirstPlayerController();
            REQUIRE(pc);
            APlayerState* ps = pc->GetPlayerState();
            REQUIRE(ps);
            const int32_t firstChars = CountLiveCharacters(f.World.get());
            const size_t controllers = f.World->GetPlayerControllers().size() + f.World->GetAIControllers().size();
            CHECK(firstChars >= 4);

            f.GM->EndMatch(ELeonTournamentMatchWinner::Draw);
            CHECK(f.GS->GetMatchState() == ELeonTournamentMatchState::Finished);
            f.GM->StartMatch();
            f.TickSeconds(0.05f);
            CHECK(pc->GetPlayerState() == ps);
            CHECK(CountLiveCharacters(f.World.get()) == firstChars);
            CHECK(f.World->GetPlayerControllers().size() + f.World->GetAIControllers().size() == controllers);
            auto* spawned = pc->GetPawn<ALeonTournamentCharacter>();
            REQUIRE(spawned);
            CHECK_FALSE(spawned->GetHealthComponent()->IsDead());
        }

        TEST_CASE("client cannot start match score or finish") {
            RegisterLeonTournamentClasses();
            auto server = UWorld::Create("AuthS");
            auto client = UWorld::Create("AuthC");
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
            gs->SetTeam1Kills(3);

            auto* clientGm = client->SpawnActor<ALeonTournamentGameMode>("ClientGM");
            client->SetGameMode(clientGm);
            clientGm->StartMatch();
            clientGm->EndMatch(ELeonTournamentMatchWinner::Team1);
            CHECK(gs->GetMatchState() == ELeonTournamentMatchState::Playing);
            CHECK(gs->GetTeam1Kills() == 3);

            auto* cgs = client->SpawnActor<ALeonTournamentGameState>("ClientGS");
            cgs->SetLocalRole(ENetRole::SimulatedProxy);
            cgs->SetTeam1Kills(99);
            cgs->SetMatchState(ELeonTournamentMatchState::Finished);
            CHECK(cgs->GetTeam1Kills() == 0);
            CHECK(cgs->GetMatchState() == ELeonTournamentMatchState::MainMenu);
        }

        TEST_CASE("bot death respawns a new pawn on the same controller") {
            FMatchWorld f;
            FLeonTournamentMatchConfig cfg;
            cfg.MaxPlayers = 2;
            cfg.StartCountdownSeconds = 0.0f;
            cfg.RespawnDelaySeconds = 0.25f;
            f.GM->SetMatchConfig(cfg);
            f.GM->StartMatch();
            f.TickSeconds(0.05f);
            auto* bot = FirstBot(*f.World);
            REQUIRE(bot);
            APlayerState* ps = bot->GetPlayerState();
            auto* oldPawn = bot->GetPawn<ALeonTournamentCharacter>();
            REQUIRE(oldPawn);
            const FUUID oldGuid = oldPawn->GetActorGuid();
            oldPawn->GetHealthComponent()->ApplyDamage(500.0f);
            f.TickSeconds(0.8f);
            auto* spawned = bot->GetPawn<ALeonTournamentCharacter>();
            REQUIRE(spawned);
            CHECK(spawned->GetActorGuid() != oldGuid);
            CHECK(f.World->FindActorByGuid(oldGuid) == nullptr);
            CHECK(bot->GetPlayerState() == ps);
            CHECK(bot->IsBehaviorTreeRunning());
            CHECK_FALSE(spawned->GetHealthComponent()->IsDead());
        }

        TEST_CASE("main menu spawns an unpossessed showcase character") {
            FMatchWorld f;
            CHECK(f.GS->GetMatchState() == ELeonTournamentMatchState::MainMenu);
            auto* showcase =
                dynamic_cast<ALeonTournamentCharacter*>(f.World->FindActorByName(kLeonTournamentMenuShowcaseActorName));
            REQUIRE(showcase);
            CHECK(showcase->GetController() == nullptr);
            CHECK(showcase->IsMenuShowcase());
            CHECK_FALSE(showcase->ShouldSpawnWeapon());
            if (auto* pc = f.World->GetFirstPlayerController()) {
                REQUIRE(pc->GetPlayerCameraManager());
                CHECK(pc->GetPlayerCameraManager()->GetCamera().GetPosition().y ==
                      doctest::Approx(kLeonTournamentMenuCameraPosition.y).epsilon(1e-3f));
            }
            f.GM->StartMatch();
            CHECK(f.World->FindActorByName(kLeonTournamentMenuShowcaseActorName) == nullptr);
        }

        TEST_CASE("sixty second death soak keeps pawn and controller counts stable") {
            FMatchWorld f;
            FLeonTournamentMatchConfig cfg;
            cfg.MaxPlayers = 4;
            cfg.StartCountdownSeconds = 0.0f;
            cfg.RespawnDelaySeconds = 0.15f;
            cfg.MatchDurationSeconds = 120.0f;
            f.GM->SetMatchConfig(cfg);
            f.GM->StartMatch();
            f.TickSeconds(0.05f);
            const int32_t expectedChars = CountLiveCharacters(f.World.get());
            const size_t expectedControllers =
                f.World->GetPlayerControllers().size() + f.World->GetAIControllers().size();
            const size_t expectedStates = f.GS->GetPlayerArray().size();
            CHECK(expectedChars >= 4);

            float elapsed = 0.0f;
            float sinceKill = 0.0f;
            while (elapsed < 60.0f) {
                const float dt = 0.05f;
                sinceKill += dt;
                if (sinceKill >= 1.0f) {
                    sinceKill = 0.0f;
                    for (const auto& actor : f.World->GetAllActors()) {
                        auto* ch = dynamic_cast<ALeonTournamentCharacter*>(actor.get());
                        if (!ch || ch->IsPendingKill() || !ch->GetHealthComponent() ||
                            ch->GetHealthComponent()->IsDead())
                            continue;
                        ch->GetHealthComponent()->ApplyDamage(500.0f);
                    }
                }
                f.World->Tick(FTimestep(dt));
                elapsed += dt;
            }

            CHECK(CountLiveCharacters(f.World.get()) == expectedChars);
            CHECK(f.World->GetPlayerControllers().size() + f.World->GetAIControllers().size() == expectedControllers);
            CHECK(f.GS->GetPlayerArray().size() == expectedStates);
            CHECK(f.World->GetGameState() == f.GS);
            CHECK(f.World->GetGameMode() == f.GM);
        }
    }

} // namespace Leon
