#include <doctest/doctest.h>

#include "Core/FTimestep.hpp"
#include "Core/FFrameProfiler.hpp"
#include "Core/FWorldUnits.hpp"
#include "Engine/UWorld.hpp"
#include "Engine/Components.hpp"
#include "Gameplay/ACharacter.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Gameplay/UCharacterMovementComponent.hpp"
#include "Gameplay/UClassRegistry.hpp"
#include "Gameplay/UGameplayStatics.hpp"
#include "Gameplay/UParticleComponent.hpp"
#include "Gameplay/EMovementMode.hpp"
#include "ALeonTournamentGameMode.hpp"
#include "ALeonTournamentGameState.hpp"
#include "ALeonTournamentCharacter.hpp"
#include "ALeonTournamentPlayerController.hpp"
#include "ALeonTournamentPlayerState.hpp"
#include "ALeonTournamentHUD.hpp"
#include "ALeonTournamentBotController.hpp"
#include "ALeonTournamentWeapon.hpp"
#include "ULeonTournamentAnimInstance.hpp"
#include "ULeonTournamentWidgets.hpp"
#include "Assets/UAssetManager.hpp"
#include "LeonTournamentTestSetup.hpp"

#include <cmath>
#include <string>
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
                World = UWorld::Create("FirstPersonArena");
                GM = World->SpawnActor<ALeonTournamentGameMode>("GM");
                World->SetGameMode(GM);
                World->InitWorld();
                GM->HUDClass = "None";
                World->BeginPlay();
                GS = GM->GetGameState();
            }

            void TickSeconds(float InSeconds) {
                const float dt = 1.0f / 30.0f;
                float acc = 0.0f;
                while (acc < InSeconds) {
                    World->Tick(FTimestep(dt));
                    acc += dt;
                }
            }
        };
    } // namespace

    TEST_SUITE("FirstPersonCameraContract") {
        TEST_CASE("pitch rotates camera only") {
            auto world = UWorld::Create("FPCam");
            world->BeginPlay();
            auto* pc = world->SpawnActor<ALeonTournamentPlayerController>("PC");
            auto* ch = world->SpawnActor<ALeonTournamentCharacter>("Char");
            world->AddPlayerController(pc);
            pc->Possess(ch);
            ch->SetThirdPerson(false);
            CHECK_FALSE(ch->IsThirdPerson());
            ch->SetControlRotation({55.0f, 0.0f, 0.0f});
            ch->Tick(0.016f);
            CHECK(ch->GetControlPitch() == doctest::Approx(55.0f));
            CHECK(std::abs(ch->GetActorRotation().x) < 0.01f);
            CHECK(std::abs(ch->GetActorRotation().z) < 0.01f);
            CHECK(glm::length(ch->GetActorUpVector() - glm::vec3(0.0f, 1.0f, 0.0f)) < 0.02f);
            if (ch->HasComponent<FCameraComponent>())
                CHECK(ch->GetComponent<FCameraComponent>().Camera.GetPitch() == doctest::Approx(55.0f));
        }

        TEST_CASE("yaw drives character and camera") {
            auto world = UWorld::Create("FPYaw");
            world->BeginPlay();
            auto* pc = world->SpawnActor<ALeonTournamentPlayerController>("PC");
            auto* ch = world->SpawnActor<ALeonTournamentCharacter>("Char");
            world->AddPlayerController(pc);
            pc->Possess(ch);
            ch->SetControlRotation({0.0f, 0.0f, 0.0f});
            ch->Tick(0.016f);
            glm::vec3 fwd = ch->GetControlPlanarForward();
            CHECK(fwd.x == doctest::Approx(1.0f).epsilon(0.05f));
            CHECK(std::abs(ch->GetActorRotation().x) < 0.01f);
        }

        TEST_CASE("third person aim ray matches camera when looking down") {
            auto world = UWorld::Create("TPAim");
            world->BeginPlay();
            auto* pc = world->SpawnActor<ALeonTournamentPlayerController>("PC");
            auto* ch = world->SpawnActor<ALeonTournamentCharacter>("Char");
            world->AddPlayerController(pc);
            pc->Possess(ch);
            ch->SetThirdPerson(true);
            ch->SetActorLocation({0.0f, 1.7f, 0.0f});
            ch->SetControlYaw(90.0f);
            ch->SetControlPitch(-55.0f);
            ch->Tick(0.016f);

            glm::vec3 viewLoc, viewFwd;
            ch->GetViewPoint(viewLoc, viewFwd);
            glm::vec3 aimOrig, aimDir;
            ch->GetAimRay(aimOrig, aimDir);
            CHECK(glm::length(aimOrig - viewLoc) < 0.02f);
            CHECK(glm::dot(glm::normalize(aimDir), glm::normalize(viewFwd)) > 0.999f);
            CHECK(glm::length(aimOrig - ch->GetActorLocation()) > 0.5f);
        }

        TEST_CASE("held weapon sways over time") {
            auto world = UWorld::Create("WeapSway");
            world->BeginPlay();
            auto* pc = world->SpawnActor<ALeonTournamentPlayerController>("PC");
            auto* ch = world->SpawnActor<ALeonTournamentCharacter>("Char");
            world->AddPlayerController(pc);
            pc->Possess(ch);
            ch->Tick(0.016f);
            auto* weap = ch->GetWeapon();
            REQUIRE(weap);
            weap->Tick(0.016f);
            const glm::vec3 a = weap->GetActorLocation();
            weap->Tick(0.35f);
            const glm::vec3 b = weap->GetActorLocation();
            CHECK(glm::length(a - b) > 0.002f);
        }
    }

    TEST_SUITE("LocomotionDirection8Way") {
        TEST_CASE("idle forward right back left and diagonals") {
            const glm::vec3 forward = FWorldUnits::PlanarForwardFromYaw(0.0f);
            CHECK(ACharacter::ComputeLocomotionDirection(glm::vec3(0.0f), forward) == doctest::Approx(0.0f));
            CHECK(ACharacter::ComputeLocomotionDirection(forward, forward) == doctest::Approx(0.0f).epsilon(0.5f));
            glm::vec3 right(-forward.z, 0.0f, forward.x);
            CHECK(ACharacter::ComputeLocomotionDirection(right, forward) == doctest::Approx(90.0f).epsilon(0.5f));
            CHECK(ACharacter::ComputeLocomotionDirection(-forward, forward) == doctest::Approx(180.0f).epsilon(0.5f));
            CHECK(ACharacter::ComputeLocomotionDirection(-right, forward) == doctest::Approx(-90.0f).epsilon(0.5f));
            glm::vec3 fr = glm::normalize(forward + right);
            CHECK(ACharacter::ComputeLocomotionDirection(fr, forward) == doctest::Approx(45.0f).epsilon(1.0f));
            glm::vec3 br = glm::normalize(-forward + right);
            CHECK(ACharacter::ComputeLocomotionDirection(br, forward) == doctest::Approx(135.0f).epsilon(1.0f));
            glm::vec3 bl = glm::normalize(-forward - right);
            CHECK(ACharacter::ComputeLocomotionDirection(bl, forward) == doctest::Approx(-135.0f).epsilon(1.0f));
            glm::vec3 fl = glm::normalize(forward - right);
            CHECK(ACharacter::ComputeLocomotionDirection(fl, forward) == doctest::Approx(-45.0f).epsilon(1.0f));
        }

        TEST_CASE("character movement writes speed and direction") {
            auto world = UWorld::Create("LocomotionRep");
            world->BeginPlay();
            auto* ch = world->SpawnActor<ALeonTournamentCharacter>("Mover");
            ch->SetControlYaw(0.0f);
            ch->Tick(0.016f);
            CHECK(ch->GetAnimRepState().Speed == doctest::Approx(0.0f).epsilon(1.0f));
            if (auto move = ch->GetCharacterMovement()) {
                move->SetVelocity({6.0f, 0.0f, 0.0f});
            }
            ch->Tick(0.016f);
            CHECK(ch->GetAnimRepState().Speed == doctest::Approx(600.0f).epsilon(5.0f));
            CHECK(ch->GetAnimRepState().Direction == doctest::Approx(0.0f).epsilon(2.0f));
            if (auto move = ch->GetCharacterMovement())
                move->SetVelocity({0.0f, 0.0f, 6.0f});
            ch->Tick(0.016f);
            CHECK(ch->GetAnimRepState().Direction == doctest::Approx(90.0f).epsilon(2.0f));
        }
    }

    TEST_SUITE("LocomotionBlendSpaceAsset") {
        TEST_CASE("eight direction samples at run speed") {
            Test::BindLeonTournamentProject();
            auto skel = UAssetManager::GetSkeleton("/Game/Skeletons/YBot.lskeleton");
            REQUIRE(skel);
            auto bs = ULeonTournamentAnimInstance::BuildLocomotionBlendSpace(skel);
            REQUIRE(bs);
            REQUIRE_FALSE(bs->GetSamples().empty());
            CHECK(ULeonTournamentAnimInstance::LocomotionBlendCoversEightDirections(*bs));

            bool hasWalkRight = false;
            bool hasForward = false;
            bool hasRight = false;
            bool hasForwardLeft = false;
            bool hasForwardRightSample = false;
            for (const auto& sample : bs->GetSamples()) {
                if (sample.SequencePath.find("WalkForwardRight") != std::string::npos)
                    hasWalkRight = true;
                if (sample.Coord.x < 300.0f)
                    continue;
                if (sample.SequencePath.find("RunForward.lanim") != std::string::npos &&
                    std::abs(sample.Coord.y) <= 6.0f)
                    hasForward = true;
                if (sample.SequencePath.find("RunRight.lanim") != std::string::npos)
                    hasRight = true;
                if (sample.SequencePath.find("RunForwardLeft.lanim") != std::string::npos)
                    hasForwardLeft = true;
                if (std::abs(sample.Coord.y - 45.0f) <= 6.0f)
                    hasForwardRightSample = true;
            }
            CHECK_FALSE(hasWalkRight);
            CHECK(hasForward);
            CHECK(hasRight);
            CHECK(hasForwardLeft);
            CHECK_FALSE(hasForwardRightSample);

            std::vector<float> w;
            bs->EvaluateWeights({600.0f, 45.0f}, w);
            float forwardW = 0.0f;
            float rightW = 0.0f;
            float backW = 0.0f;
            for (size_t i = 0; i < bs->GetSamples().size(); ++i) {
                const auto& s = bs->GetSamples()[i];
                if (s.SequencePath.find("RunForward.lanim") != std::string::npos)
                    forwardW += w[i];
                if (s.SequencePath.find("RunRight.lanim") != std::string::npos)
                    rightW += w[i];
                if (s.SequencePath.find("RunBackward.lanim") != std::string::npos)
                    backW += w[i];
            }
            CHECK(forwardW > 0.15f);
            CHECK(rightW > 0.15f);
            CHECK(forwardW + rightW > backW * 3.0f);
        }
    }

    TEST_SUITE("DeathRespawnStanding") {
        TEST_CASE("kill respawns walking upright idle") {
            FMatchWorld f;
            FLeonTournamentMatchConfig cfg;
            cfg.MaxPlayers = 2;
            cfg.StartCountdownSeconds = 0.0f;
            cfg.RespawnDelaySeconds = 0.2f;
            f.GM->SetMatchConfig(cfg);
            f.GM->StartMatch();
            f.GS->SetMatchState(ELeonTournamentMatchState::Playing);

            auto* ch = f.World->SpawnActor<ALeonTournamentCharacter>("Victim");
            auto* ps = f.World->SpawnActor<ALeonTournamentPlayerState>("VPS");
            auto* pc = f.World->SpawnActor<ALeonTournamentPlayerController>("VPC");
            ps->SetTeam(ELeonTournamentTeam::Team1);
            pc->SetPlayerState(ps);
            pc->Possess(ch);
            ch->SetActorLocation({0.0f, 1.7f, 0.0f});
            ch->GetHealthComponent()->ApplyDamage(500.0f);
            CHECK(ch->GetHealthComponent()->IsDead());
            CHECK(ch->GetCharacterMovement()->GetMovementMode() == EMovementMode::None);

            const FUUID oldGuid = ch->GetActorGuid();
            f.GM->RespawnCharacter(*ch);
            auto* spawned = pc->GetPawn<ALeonTournamentCharacter>();
            REQUIRE(spawned);
            CHECK(spawned->GetActorGuid() != oldGuid);
            CHECK(f.World->FindActorByGuid(oldGuid) == nullptr);
            CHECK_FALSE(spawned->GetHealthComponent()->IsDead());
            CHECK(spawned->GetHealthComponent()->GetHealth() ==
                  doctest::Approx(spawned->GetHealthComponent()->GetMaxHealth()));
            CHECK(spawned->GetCharacterMovement()->GetMovementMode() == EMovementMode::Walking);
            CHECK_FALSE(spawned->GetCharacterMovement()->IsFalling());
            CHECK(glm::length(spawned->GetCharacterMovement()->GetVelocity()) < 0.05f);
            CHECK(std::abs(spawned->GetActorRotation().x) < 0.01f);
            CHECK(std::abs(spawned->GetActorRotation().z) < 0.01f);
            CHECK(glm::length(spawned->GetActorUpVector() - glm::vec3(0.0f, 1.0f, 0.0f)) < 0.02f);
            if (auto anim = spawned->GetMesh() ? spawned->GetMesh()->GetAnimInstance() : nullptr) {
                anim->NativeUpdateAnimation(0.016f);
                CHECK(anim->GetStateMachine().GetCurrentState() != "Death");
                CHECK(anim->GetFloat("Speed") == doctest::Approx(0.0f).epsilon(1.0f));
            }
        }
    }

    TEST_SUITE("HitscanVfxAndHitConfirm") {
        TEST_CASE("successful damage spawns vfx and hit marker") {
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
            attacker->SetActorLocation({0.0f, 1.7f, 0.0f});
            attacker->SetControlYaw(90.0f);
            attacker->SetControlPitch(0.0f);
            attacker->Tick(0.016f);
            glm::vec3 origin, dir;
            attacker->GetAimRay(origin, dir);
            victim->SetActorLocation(origin + dir * 4.0f);
            REQUIRE(attacker->GetWeapon());
            const float hp = victim->GetHealthComponent()->GetHealth();
            CHECK(attacker->GetWeapon()->ServerFire());
            CHECK(victim->GetHealthComponent()->GetHealth() < hp);
            CHECK(pca->IsHitMarkerActive());
            CHECK(attacker->GetWeapon()->GetLastVfxSpawnCount() >= 2);
        }

        TEST_CASE("miss still creates muzzle and tracer") {
            FMatchWorld f;
            f.GS->SetMatchState(ELeonTournamentMatchState::Playing);
            auto* ch = f.World->SpawnActor<ALeonTournamentCharacter>("Shooter");
            REQUIRE(ch->GetWeapon());
            ch->SetControlPitch(-80.0f);
            CHECK(ch->GetWeapon()->ServerFire());
            CHECK(ch->GetWeapon()->GetLastVfxSpawnCount() >= 2);
        }

        TEST_CASE("generic emitter burst lives then dies") {
            auto world = UWorld::Create("VfxWorld");
            world->BeginPlay();
            FParticleEmitterSettings settings;
            settings.BurstCount = 4;
            settings.Lifetime = 0.05f;
            CHECK(UGameplayStatics::SpawnEmitterAtLocation(world.get(), settings, {0.0f, 1.0f, 0.0f}));
            CHECK(world->GetTransientParticleCount() == 4);
            world->Tick(FTimestep(0.02f));
            CHECK(world->GetTransientParticleCount() > 0);
        }

        TEST_CASE("emitter burst count is clamped") {
            auto world = UWorld::Create("VfxBurstCap");
            world->BeginPlay();
            FParticleEmitterSettings settings;
            settings.BurstCount = 200;
            settings.Lifetime = 0.2f;
            CHECK(UGameplayStatics::SpawnEmitterAtLocation(world.get(), settings, {0.0f, 1.0f, 0.0f}));
            CHECK(world->GetTransientParticleCount() == kMaxBurstParticles);
        }
    }

    TEST_SUITE("Match2v2Rules") {
        TEST_CASE("offline fill is 2 per team") {
            FMatchWorld f;
            CHECK(f.GM->GetMatchConfig().MaxPlayers == 12);
            CHECK(f.GM->GetMatchConfig().MaxTeamSize == 6);
            CHECK(f.GM->GetMatchConfig().ScoreLimit == 15);
            CHECK(f.GM->GetMatchConfig().MatchDurationSeconds == doctest::Approx(420.0f));
            CHECK(f.GM->GetMatchConfig().RespawnDelaySeconds == doctest::Approx(1.0f));
            CHECK(f.GM->GetMatchConfig().SpawnProtectionSeconds == doctest::Approx(2.5f));
            f.GM->EnterLobby();
            CHECK(f.GM->CountBotsOnTeam(ELeonTournamentTeam::Team1) == 2);
            CHECK(f.GM->CountBotsOnTeam(ELeonTournamentTeam::Team2) == 2);
        }

        TEST_CASE("score limit 15 ends the match") {
            FMatchWorld f;
            f.GS->SetMatchState(ELeonTournamentMatchState::Playing);
            auto* killer = f.World->SpawnActor<ALeonTournamentCharacter>("K");
            auto* victim = f.World->SpawnActor<ALeonTournamentCharacter>("V");
            auto* psk = f.World->SpawnActor<ALeonTournamentPlayerState>("PSK");
            auto* psv = f.World->SpawnActor<ALeonTournamentPlayerState>("PSV");
            psk->SetTeam(ELeonTournamentTeam::Team1);
            psv->SetTeam(ELeonTournamentTeam::Team2);
            auto* pck = f.World->SpawnActor<ALeonTournamentPlayerController>("PCK");
            auto* pcv = f.World->SpawnActor<ALeonTournamentPlayerController>("PCV");
            pck->SetPlayerState(psk);
            pcv->SetPlayerState(psv);
            pck->Possess(killer);
            pcv->Possess(victim);
            f.GS->SetTeam1Kills(14);
            FDamageInfo info;
            info.DamageAmount = 200.0f;
            info.Instigator = killer;
            REQUIRE(f.GM->ApplyAuthoritativeDamage(*killer, *victim, info));
            CHECK(victim->GetHealthComponent()->IsDead());
            CHECK(f.GS->GetMatchState() == ELeonTournamentMatchState::Finished);
            CHECK(f.GS->GetMatchWinner() == ELeonTournamentMatchWinner::Team1);
        }
    }

    TEST_SUITE("JumpFallLand") {
        TEST_CASE("jump sets falling then walking") {
            auto world = UWorld::Create("JumpFP");
            world->BeginPlay();
            auto* ch = world->SpawnActor<ALeonTournamentCharacter>("Jumper");
            REQUIRE(ch->GetCharacterMovement());
            CHECK(ch->GetCharacterMovement()->IsMovingOnGround());
            ch->Jump();
            ch->Tick(0.016f);
            CHECK(ch->GetCharacterMovement()->IsFalling());
            for (int i = 0; i < 90; ++i)
                ch->Tick(0.016f);
            CHECK(ch->GetCharacterMovement()->IsMovingOnGround());
            CHECK_FALSE(ch->GetCharacterMovement()->IsFalling());
        }
    }

    TEST_SUITE("TwoVTwoCpuProfile") {
        TEST_CASE("2v2 match ticks collect a frame profile") {
            FMatchWorld f;
            FLeonTournamentMatchConfig cfg;
            cfg.StartCountdownSeconds = 0.0f;
            f.GM->SetMatchConfig(cfg);
            f.GM->StartMatch();
            FFrameProfiler::BeginFrame();
            f.TickSeconds(1.0f);
            FFrameProfiler::EndFrame(16.0f);
            CHECK(f.World->GetAIControllers().size() >= 3);
            int32_t fighters = 0;
            for (const auto& actor : f.World->GetAllActors()) {
                if (dynamic_cast<ALeonTournamentCharacter*>(actor.get()))
                    ++fighters;
            }
            CHECK(fighters >= 4);
        }
    }

} // namespace Leon
