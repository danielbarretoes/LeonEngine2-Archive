#include "ALeonTournamentBotController.hpp"
#include "ALeonTournamentCharacter.hpp"
#include "ALeonTournamentPlayerState.hpp"
#include "ALeonTournamentWeapon.hpp"
#include "ALeonTournamentPickup.hpp"
#include "ALeonTournamentGameState.hpp"
#include "ALeonTournamentGameMode.hpp"
#include "ULeonTournamentGameInstance.hpp"
#include "Engine/UEngine.hpp"
#include "Engine/UWorld.hpp"
#include "Renderer/FDebugRenderer.hpp"
#include "Gameplay/UGameplayStatics.hpp"
#include "Gameplay/FGameplayDebugger.hpp"
#include "Gameplay/UCharacterMovementComponent.hpp"
#include "AI/UBehaviorTree.hpp"
#include "AI/UPathFollowingComponent.hpp"
#include "AI/UAIPerceptionComponent.hpp"
#include "Core/FWorldUnits.hpp"
#include "Core/FInput.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace Leon {

    namespace {
        constexpr float kLastKnownMemory = 4.0f;
        constexpr float kMinRange = 7.0f;
        constexpr float kMaxRange = 30.0f;

        float PlanarDistance(const glm::vec3& A, const glm::vec3& B) {
            glm::vec3 d = A - B;
            d.y = 0.0f;
            return glm::length(d);
        }

        float AngleDegrees(const glm::vec3& A, const glm::vec3& B) {
            const float la = glm::length(A);
            const float lb = glm::length(B);
            if (la < 1e-5f || lb < 1e-5f)
                return 180.0f;
            return glm::degrees(std::acos(std::clamp(glm::dot(A / la, B / lb), -1.0f, 1.0f)));
        }
    } // namespace

    ALeonTournamentBotController::ALeonTournamentBotController(entt::entity InHandle, UWorld* InWorld,
                                                               const std::string& InName)
        : AAIController(InHandle, InWorld, InName) {
        SetClass("ALeonTournamentBotController");
    }

    ALeonTournamentCharacter* ALeonTournamentBotController::GetCurrentTarget() const {
        if (!Blackboard)
            return nullptr;
        return static_cast<ALeonTournamentCharacter*>(Blackboard->GetValueAsObject("TargetActor"));
    }

    ELeonTournamentBotState ALeonTournamentBotController::GetBotState() const {
        return CachedState;
    }

    bool ALeonTournamentBotController::HasLineOfSight(ALeonTournamentCharacter& InTarget) const {
        if (Perception)
            return Perception->HasLineOfSight(InTarget);
        return HasLineOfSightTo(InTarget);
    }

    void ALeonTournamentBotController::AssignPersonality() {
        int32_t id = 1;
        if (auto* ps = GetPlayerState())
            id = std::max(1, ps->GetPlayerId());
        Personality.Aggression = 0.35f + static_cast<float>(id % 5) * 0.12f;
        Personality.Accuracy = 0.38f + static_cast<float>((id * 3) % 5) * 0.08f;
        Personality.ReactionTime = 0.18f + static_cast<float>((id * 7) % 4) * 0.07f;
        Personality.PreferredRange = 10.0f + static_cast<float>((id * 11) % 6) * 2.5f;
        Personality.StrafeFrequency = 0.7f + static_cast<float>((id * 13) % 5) * 0.25f;
        Personality.CoverPreference = 0.25f + static_cast<float>((id * 17) % 5) * 0.14f;
        Personality.Tactic = id % 3;

        ELeonTournamentBotDifficulty difficulty = ELeonTournamentBotDifficulty::Normal;
        if (UEngine::HasInstance()) {
            if (auto* gi = dynamic_cast<ULeonTournamentGameInstance*>(UEngine::Get().GetGameInstance().get()))
                difficulty = gi->GetBotDifficulty();
        }
        switch (difficulty) {
        case ELeonTournamentBotDifficulty::Casual:
            Personality.Accuracy *= 0.72f;
            Personality.ReactionTime *= 1.35f;
            Personality.Aggression *= 0.85f;
            break;
        case ELeonTournamentBotDifficulty::Hard:
            Personality.Accuracy = std::min(0.95f, Personality.Accuracy * 1.28f);
            Personality.ReactionTime = std::max(0.08f, Personality.ReactionTime * 0.72f);
            Personality.Aggression = std::min(0.95f, Personality.Aggression * 1.25f);
            Personality.CoverPreference *= 0.75f;
            break;
        case ELeonTournamentBotDifficulty::Normal:
        default:
            break;
        }
    }

    void ALeonTournamentBotController::SelectCombatWeapon(ALeonTournamentCharacter& InSelf, float InDistance) {
        if (InDistance < 8.0f && InSelf.HasWeapon(ELeonTournamentWeaponId::Shotgun))
            InSelf.SelectWeapon(ELeonTournamentWeaponId::Shotgun);
        else if (InDistance < 26.0f && InSelf.HasWeapon(ELeonTournamentWeaponId::Rocket))
            InSelf.SelectWeapon(ELeonTournamentWeaponId::Rocket);
        else if (InSelf.HasWeapon(ELeonTournamentWeaponId::Rifle))
            InSelf.SelectWeapon(ELeonTournamentWeaponId::Rifle);
    }

    void ALeonTournamentBotController::TickPickupScan() {
        auto board = Blackboard;
        auto* self = GetPawn<ALeonTournamentCharacter>();
        if (!board || !self || !World)
            return;

        board->SetValueAsBool("WantsPickup", false);
        board->SetValueAsVector("PickupLocation", glm::vec3(0.0f));
        board->SetValueAsInt("PickupKind", 0);

        const glm::vec3 origin = self->GetActorLocation();
        const bool bLow = self->GetHealthComponent() &&
                          self->GetHealthComponent()->GetHealth() < self->GetHealthComponent()->GetMaxHealth() * 0.35f;

        float bestDist = 18.0f;
        glm::vec3 bestLoc{0.0f};
        int32_t bestKind = 0;

        for (const auto& actor : World->GetAllActors()) {
            if (!actor || actor->IsPendingKill())
                continue;
            if (auto* health = dynamic_cast<ALeonTournamentHealthPickup*>(actor.get())) {
                if (!bLow)
                    continue;
                const float d = PlanarDistance(origin, health->GetActorLocation());
                if (d < bestDist) {
                    bestDist = d;
                    bestLoc = health->GetActorLocation();
                    bestKind = 1;
                }
                continue;
            }
            if (auto* weapon = dynamic_cast<ALeonTournamentWeaponPickup*>(actor.get())) {
                if (self->HasWeapon(weapon->GetWeaponId()))
                    continue;
                const float d = PlanarDistance(origin, weapon->GetActorLocation());
                if (d < bestDist) {
                    bestDist = d;
                    bestLoc = weapon->GetActorLocation();
                    bestKind = 2;
                }
            }
        }

        if (bestKind > 0) {
            board->SetValueAsBool("WantsPickup", true);
            board->SetValueAsVector("PickupLocation", bestLoc);
            board->SetValueAsInt("PickupKind", bestKind);
        }
    }

    void ALeonTournamentBotController::Possess(APawn* InPawn) {
        if (GetPawn() != InPawn)
            bDamageBound = false;
        AAIController::Possess(InPawn);
        AssignPersonality();
        auto* ch = GetPawn<ALeonTournamentCharacter>();
        if (!ch || !ch->GetHealthComponent() || bDamageBound)
            return;
        ch->GetHealthComponent()->OnDamage.push_back([this](const FDamageInfo& info) { NotifyDamaged(info); });
        bDamageBound = true;
    }

    void ALeonTournamentBotController::NotifyDamaged(const FDamageInfo& InInfo) {
        auto* attacker = dynamic_cast<ALeonTournamentCharacter*>(InInfo.Instigator);
        auto* self = GetPawn<ALeonTournamentCharacter>();
        if (!attacker || !self || attacker == self || !Blackboard)
            return;
        if (self->GetTeam() != ELeonTournamentTeam::None && attacker->GetTeam() == self->GetTeam()) {
            if (auto* gm = dynamic_cast<ALeonTournamentGameMode*>(World ? World->GetGameMode() : nullptr)) {
                if (gm->GetActiveGameMode() != ELeonTournamentGameModeId::FreeForAll)
                    return;
            } else {
                return;
            }
        }
        Blackboard->SetValueAsObject("TargetActor", attacker);
        Blackboard->SetValueAsBool("HasTarget", true);
        Blackboard->SetValueAsVector("LastKnownTargetLocation", attacker->GetActorLocation());
        if (Perception)
            Perception->RememberActor(attacker);
        AcquireTime = 0.0f;
    }

    void ALeonTournamentBotController::NotifyRespawned() {
        if (Perception)
            Perception->ForgetLastKnown();
        AcquireTime = 0.0f;
        StrafeTimer = 0.0f;
        LookAroundTimer = 0.0f;
        bHasLastPatrolGoal = false;
        CachedState = ELeonTournamentBotState::Respawn;
        if (Blackboard && BoardAsset)
            Blackboard->InitializeFrom(BoardAsset);
        else if (Blackboard) {
            Blackboard->SetValueAsBool("IsDead", false);
            Blackboard->SetValueAsObject("TargetActor", nullptr);
            Blackboard->SetValueAsBool("HasTarget", false);
            Blackboard->SetValueAsBool("HasLineOfSight", false);
            Blackboard->SetValueAsBool("IsLowHealth", false);
            Blackboard->SetValueAsBool("HasAmmo", true);
            Blackboard->SetValueAsBool("IsReloading", false);
            Blackboard->SetValueAsBool("HasCover", false);
            Blackboard->SetValueAsBool("HasLastKnown", false);
            Blackboard->SetValueAsVector("TargetLocation", glm::vec3(0.0f));
            Blackboard->SetValueAsVector("LastKnownTargetLocation", glm::vec3(0.0f));
            Blackboard->SetValueAsVector("DesiredLocation", glm::vec3(0.0f));
            Blackboard->SetValueAsFloat("DistanceToTarget", 0.0f);
        }
        if (Brain && Tree) {
            Brain->StopTree();
            Brain->StartTree(Tree);
        }
        StopMovement();
    }

    bool ALeonTournamentBotController::IsBehaviorTreeRunning() const {
        return Brain && Brain->IsRunning();
    }

    glm::vec3 ALeonTournamentBotController::PickApproachLocation(const glm::vec3& InFrom, const glm::vec3& InTarget,
                                                                 float InRange) const {
        glm::vec3 delta = InFrom - InTarget;
        delta.y = 0.0f;
        float dist = glm::length(delta);
        if (dist < 1e-3f)
            delta = glm::vec3(1.0f, 0.0f, 0.0f);
        else
            delta /= dist;
        const float sign = Personality.Tactic == 1 ? ((StrafeSign > 0.0f) ? 1.0f : -1.0f) : 1.0f;
        glm::vec3 side(-delta.z, 0.0f, delta.x);
        glm::vec3 pos = InTarget + delta * InRange + side * (Personality.Tactic == 1 ? 4.0f * sign : 0.0f);
        pos.y = InFrom.y;
        return pos;
    }

    glm::vec3 ALeonTournamentBotController::PickCoverLocation(ALeonTournamentCharacter& InSelf,
                                                              ALeonTournamentCharacter* InTarget) const {
        glm::vec3 best = InSelf.GetActorLocation();
        float bestScore = -1e9f;
        for (const glm::vec3& p : CoverPoints) {
            const float d = PlanarDistance(InSelf.GetActorLocation(), p);
            if (d > 35.0f)
                continue;
            float score = 20.0f - d;
            if (InTarget && World) {
                FHitResult hit;
                const bool bHit =
                    World->LineTraceSingleByChannel(p + glm::vec3(0.0f, 0.4f, 0.0f), InTarget->GetActorLocation(),
                                                    ECollisionChannel::Visibility, &InSelf, hit);
                if (bHit && hit.Actor != InTarget)
                    score += 25.0f;
            }
            if (score > bestScore) {
                bestScore = score;
                best = p;
            }
        }
        return best;
    }

    glm::vec3 ALeonTournamentBotController::PickPatrolLocation(ALeonTournamentCharacter& InSelf) const {
        if (Waypoints.empty())
            return InSelf.GetActorLocation() + glm::vec3(4.0f, 0.0f, 0.0f);
        const glm::vec3 from = InSelf.GetActorLocation();
        std::vector<size_t> ranked;
        ranked.reserve(Waypoints.size());
        for (size_t i = 0; i < Waypoints.size(); ++i) {
            const float d = PlanarDistance(from, Waypoints[i]);
            if (d < 3.0f)
                continue;
            if (bHasLastPatrolGoal && PlanarDistance(Waypoints[i], LastPatrolGoal) < 2.5f)
                continue;
            ranked.push_back(i);
        }
        if (ranked.empty()) {
            for (size_t i = 0; i < Waypoints.size(); ++i)
                ranked.push_back(i);
        }
        std::sort(ranked.begin(), ranked.end(), [&](size_t a, size_t b) {
            return PlanarDistance(from, Waypoints[a]) > PlanarDistance(from, Waypoints[b]);
        });
        const size_t pickCount = std::min<size_t>(3, ranked.size());
        const size_t idx = ranked[static_cast<size_t>(rand()) % pickCount];
        LastPatrolGoal = Waypoints[idx];
        bHasLastPatrolGoal = true;
        return LastPatrolGoal;
    }

    void ALeonTournamentBotController::TickAim(float DeltaSeconds, const glm::vec3& InWorldPoint) {
        auto* pawn = GetPawn<ALeonTournamentCharacter>();
        if (!pawn)
            return;
        glm::vec3 delta = InWorldPoint - pawn->GetActorLocation();
        const float len = glm::length(delta);
        if (len < 1e-4f)
            return;
        glm::vec3 n = delta / len;
        const float error = (1.0f - Personality.Accuracy) * 0.04f * std::min(len, 25.0f);
        n.x += error * StrafeSign * 0.15f;
        n.z += error * 0.1f;
        n = glm::normalize(n);
        const float wantYaw = glm::degrees(std::atan2(n.z, n.x));
        const float wantPitch = glm::degrees(std::asin(std::clamp(n.y, -1.0f, 1.0f)));
        glm::vec3 cur = pawn->GetControlRotation();
        const float rate = 8.0f / std::max(Personality.ReactionTime, 0.08f);
        const float a = 1.0f - std::exp(-rate * DeltaSeconds);
        auto lerpAngle = [](float from, float to, float t) {
            float d = to - from;
            while (d > 180.0f)
                d -= 360.0f;
            while (d < -180.0f)
                d += 360.0f;
            return from + d * t;
        };
        pawn->SetControlRotation({lerpAngle(cur.x, wantPitch, a), lerpAngle(cur.y, wantYaw, a), 0.0f});
        pawn->ApplyYawOnlyActorRotation();
    }

    bool ALeonTournamentBotController::IsAimAligned(const glm::vec3& InWorldPoint, float InDegrees) const {
        auto* pawn = GetPawn<ALeonTournamentCharacter>();
        if (!pawn)
            return false;
        glm::vec3 to = InWorldPoint - pawn->GetActorLocation();
        if (glm::length(to) < 1e-4f)
            return true;
        return AngleDegrees(pawn->GetControlLookDirection(), to) <= InDegrees;
    }

    void ALeonTournamentBotController::TickPerception() {
        auto board = Blackboard;
        auto* self = GetPawn<ALeonTournamentCharacter>();
        if (!board || !self)
            return;

        const bool bDead = self->GetHealthComponent() && self->GetHealthComponent()->IsDead();
        board->SetValueAsBool("IsDead", bDead);
        auto* weapon = self->GetWeapon();
        board->SetValueAsBool("HasAmmo", weapon && weapon->GetCurrentAmmo() > 0);
        board->SetValueAsBool("IsReloading", weapon && weapon->IsReloading());
        const bool bLow = self->GetHealthComponent() &&
                          self->GetHealthComponent()->GetHealth() < self->GetHealthComponent()->GetMaxHealth() * 0.35f;
        board->SetValueAsBool("IsLowHealth", bLow);

        if (Perception)
            Perception->UpdatePerception();
    }

    void ALeonTournamentBotController::BuildBehaviorTree() {
        BoardAsset = MakeRef<UBlackboardData>("LeonTournamentBotBB");
        BoardAsset->AddKey({"TargetActor", EBlackboardKeyType::Actor, static_cast<void*>(nullptr)});
        BoardAsset->AddKey({"TargetLocation", EBlackboardKeyType::Vector, glm::vec3(0.0f)});
        BoardAsset->AddKey({"LastKnownTargetLocation", EBlackboardKeyType::Vector, glm::vec3(0.0f)});
        BoardAsset->AddKey({"DesiredLocation", EBlackboardKeyType::Vector, glm::vec3(0.0f)});
        BoardAsset->AddKey({"HasTarget", EBlackboardKeyType::Bool, false});
        BoardAsset->AddKey({"HasLineOfSight", EBlackboardKeyType::Bool, false});
        BoardAsset->AddKey({"HasLastKnown", EBlackboardKeyType::Bool, false});
        BoardAsset->AddKey({"DistanceToTarget", EBlackboardKeyType::Float, 0.0f});
        BoardAsset->AddKey({"IsLowHealth", EBlackboardKeyType::Bool, false});
        BoardAsset->AddKey({"HasAmmo", EBlackboardKeyType::Bool, true});
        BoardAsset->AddKey({"IsReloading", EBlackboardKeyType::Bool, false});
        BoardAsset->AddKey({"IsDead", EBlackboardKeyType::Bool, false});
        BoardAsset->AddKey({"HasCover", EBlackboardKeyType::Bool, false});
        BoardAsset->AddKey({"WantsPickup", EBlackboardKeyType::Bool, false});
        BoardAsset->AddKey({"PickupLocation", EBlackboardKeyType::Vector, glm::vec3(0.0f)});
        BoardAsset->AddKey({"PickupKind", EBlackboardKeyType::Int, 0});

        auto perception =
            MakeRef<UBTService_Native>("Perception", [this](UBehaviorTreeComponent&, float) { TickPerception(); });
        perception->Interval = 0.12f;
        perception->TimeAccumulator = perception->Interval;

        auto pickupScan =
            MakeRef<UBTService_Native>("PickupScan", [this](UBehaviorTreeComponent&, float) { TickPickupScan(); });
        pickupScan->Interval = 0.25f;
        pickupScan->TimeAccumulator = pickupScan->Interval;

        auto dead = MakeRef<UBTTask_Native>("Dead", [this](UBehaviorTreeComponent& owner, float) {
            auto board = owner.GetBlackboard();
            auto* pawn = GetPawn<ALeonTournamentCharacter>();
            const bool bDead = (pawn && pawn->GetHealthComponent() && pawn->GetHealthComponent()->IsDead()) ||
                               (board && board->GetValueAsBool("IsDead"));
            if (!bDead)
                return EBTNodeResult::Failed;
            if (board)
                board->SetValueAsBool("IsDead", true);
            CachedState = ELeonTournamentBotState::Dead;
            if (pawn)
                pawn->BotSetFireHeld(false);
            StopMovement();
            return EBTNodeResult::Succeeded;
        });

        auto reload = MakeRef<UBTTask_Native>("Reload", [this](UBehaviorTreeComponent&, float) {
            auto* pawn = GetPawn<ALeonTournamentCharacter>();
            if (!pawn)
                return EBTNodeResult::Failed;
            CachedState = ELeonTournamentBotState::Reload;
            pawn->BotSetFireHeld(false);
            pawn->BotRequestReload();
            return pawn->GetWeapon() && pawn->GetWeapon()->IsReloading() ? EBTNodeResult::InProgress
                                                                         : EBTNodeResult::Succeeded;
        });

        auto cover = MakeRef<UBTTask_Native>("TakeCover", [this](UBehaviorTreeComponent& owner, float dt) {
            auto board = owner.GetBlackboard();
            auto* pawn = GetPawn<ALeonTournamentCharacter>();
            if (!board || !pawn || !board->GetValueAsBool("IsLowHealth"))
                return EBTNodeResult::Failed;
            if (CoverPoints.empty() || Personality.CoverPreference < 0.35f)
                return EBTNodeResult::Failed;
            CachedState = ELeonTournamentBotState::TakeCover;
            pawn->BotSetFireHeld(false);
            const glm::vec3 cover = PickCoverLocation(*pawn, GetCurrentTarget());
            board->SetValueAsVector("DesiredLocation", cover);
            MoveToLocation(cover, 0.8f);
            if (PlanarDistance(pawn->GetActorLocation(), cover) <= 1.0f) {
                pawn->BotRequestReload();
                return EBTNodeResult::Succeeded;
            }
            (void)dt;
            return EBTNodeResult::InProgress;
        });

        auto combat = MakeRef<UBTTask_Native>("Combat", [this](UBehaviorTreeComponent& owner, float dt) {
            auto board = owner.GetBlackboard();
            auto* pawn = GetPawn<ALeonTournamentCharacter>();
            auto* target = GetCurrentTarget();
            if (!board || !pawn || !target || !board->GetValueAsBool("HasTarget")) {
                if (pawn)
                    pawn->BotSetFireHeld(false);
                return EBTNodeResult::Failed;
            }
            CachedState = ELeonTournamentBotState::Combat;
            AcquireTime += dt;
            StrafeTimer -= dt;
            const glm::vec3 tgt = target->GetActorLocation();
            const float dist = PlanarDistance(pawn->GetActorLocation(), tgt);
            const float ideal = Personality.PreferredRange;
            SelectCombatWeapon(*pawn, dist);
            TickAim(dt, tgt);

            if (dist > kMaxRange || dist > ideal + 6.0f) {
                pawn->BotSetFireHeld(false);
                CachedState = ELeonTournamentBotState::MoveToTarget;
                const glm::vec3 dest = PickApproachLocation(pawn->GetActorLocation(), tgt, ideal);
                board->SetValueAsVector("DesiredLocation", dest);
                MoveToLocation(dest, 1.0f);
                return EBTNodeResult::InProgress;
            }
            if (dist < kMinRange) {
                pawn->BotSetFireHeld(false);
                CachedState = ELeonTournamentBotState::MoveToTarget;
                const glm::vec3 dest = PickApproachLocation(pawn->GetActorLocation(), tgt, ideal);
                MoveToLocation(dest, 1.0f);
                return EBTNodeResult::InProgress;
            }

            const bool bLos = board->GetValueAsBool("HasLineOfSight");
            if (!bLos) {
                pawn->BotSetFireHeld(false);
                CachedState = ELeonTournamentBotState::MoveToTarget;
                MoveToLocation(tgt, 1.0f);
                return EBTNodeResult::InProgress;
            }

            if (StrafeTimer <= 0.0f) {
                StrafeSign = (rand() % 2) ? 1.0f : -1.0f;
                StrafeTimer = Personality.StrafeFrequency;
                glm::vec3 to = tgt - pawn->GetActorLocation();
                to.y = 0.0f;
                if (glm::length(to) > 1e-4f)
                    to = glm::normalize(to);
                glm::vec3 right(-to.z, 0.0f, to.x);
                const float side = 2.2f + static_cast<float>(Personality.Tactic) * 0.8f;
                glm::vec3 dest = pawn->GetActorLocation() + right * (StrafeSign * side) - to * 0.4f;
                dest.y = pawn->GetActorLocation().y;
                MoveToLocation(dest, 0.7f);
            }
            glm::vec3 to = tgt - pawn->GetActorLocation();
            to.y = 0.0f;
            if (glm::length(to) > 1e-4f)
                to = glm::normalize(to);
            glm::vec3 right(-to.z, 0.0f, to.x);
            pawn->AddMovementInput(right, StrafeSign * pawn->GetMoveSpeed() * 0.35f);

            const float align = 14.0f + (1.0f - Personality.Accuracy) * 12.0f;
            const bool bReady = AcquireTime >= Personality.ReactionTime && bLos && IsAimAligned(tgt, align);
            pawn->BotSetFireHeld(bReady);
            if (bReady)
                CachedState = ELeonTournamentBotState::Fire;
            return EBTNodeResult::InProgress;
        });

        auto pickup = MakeRef<UBTTask_Native>("Pickup", [this](UBehaviorTreeComponent& owner, float) {
            auto board = owner.GetBlackboard();
            auto* pawn = GetPawn<ALeonTournamentCharacter>();
            if (!board || !pawn || !board->GetValueAsBool("WantsPickup"))
                return EBTNodeResult::Failed;
            if (board->GetValueAsBool("HasTarget") && board->GetValueAsFloat("DistanceToTarget") < 14.0f)
                return EBTNodeResult::Failed;
            CachedState = ELeonTournamentBotState::Idle;
            pawn->BotSetFireHeld(false);
            const glm::vec3 dest = board->GetValueAsVector("PickupLocation");
            MoveToLocation(dest, 1.0f);
            if (PlanarDistance(pawn->GetActorLocation(), dest) <= 1.4f) {
                SelectCombatWeapon(*pawn, 999.0f);
                return EBTNodeResult::Succeeded;
            }
            return EBTNodeResult::InProgress;
        });

        auto search = MakeRef<UBTTask_Native>("Search", [this](UBehaviorTreeComponent& owner, float dt) {
            auto board = owner.GetBlackboard();
            auto* pawn = GetPawn<ALeonTournamentCharacter>();
            if (!board || !pawn || !board->GetValueAsBool("HasLastKnown"))
                return EBTNodeResult::Failed;
            CachedState = ELeonTournamentBotState::Search;
            pawn->BotSetFireHeld(false);
            const glm::vec3 dest = board->GetValueAsVector("LastKnownTargetLocation");
            MoveToLocation(dest, 1.0f);
            LookAroundTimer += dt;
            TickAim(dt, dest + glm::vec3(std::sin(LookAroundTimer * 2.0f) * 2.0f, 0.0f,
                                         std::cos(LookAroundTimer * 1.7f) * 2.0f));
            if (PlanarDistance(pawn->GetActorLocation(), dest) < 1.2f && LookAroundTimer > 1.6f) {
                if (Perception)
                    Perception->ForgetLastKnown();
                board->SetValueAsBool("HasLastKnown", false);
                return EBTNodeResult::Succeeded;
            }
            return EBTNodeResult::InProgress;
        });

        auto patrol = MakeRef<UBTTask_Native>("Patrol", [this](UBehaviorTreeComponent& owner, float dt) {
            auto* pawn = GetPawn<ALeonTournamentCharacter>();
            auto board = owner.GetBlackboard();
            if (!pawn || !board)
                return EBTNodeResult::Failed;
            CachedState = ELeonTournamentBotState::Idle;
            pawn->BotSetFireHeld(false);
            glm::vec3 dest = board->GetValueAsVector("DesiredLocation");
            if (PlanarDistance(pawn->GetActorLocation(), dest) < 1.2f || glm::length(dest) < 0.01f) {
                dest = PickPatrolLocation(*pawn);
                board->SetValueAsVector("DesiredLocation", dest);
            }
            MoveToLocation(dest, 1.1f);
            TickAim(dt, dest);
            return EBTNodeResult::InProgress;
        });

        auto reloadSeq = MakeRef<UBTComposite_Sequence>("NeedReload");
        reloadSeq->Decorators.push_back(MakeRef<UBTDecorator_Blackboard>("HasAmmo", false));
        reloadSeq->AddChild(reload);

        auto coverSeq = MakeRef<UBTComposite_Sequence>("NeedCover");
        coverSeq->Decorators.push_back(MakeRef<UBTDecorator_Blackboard>("IsLowHealth", true));
        coverSeq->AddChild(cover);

        auto pickupSeq = MakeRef<UBTComposite_Sequence>("WantPickup");
        pickupSeq->Decorators.push_back(MakeRef<UBTDecorator_Blackboard>("WantsPickup", true));
        pickupSeq->AddChild(pickup);

        auto root = MakeRef<UBTComposite_Selector>("Selector");
        root->Services.push_back(perception);
        root->Services.push_back(pickupScan);
        root->AddChild(dead);
        root->AddChild(reloadSeq);
        root->AddChild(coverSeq);
        root->AddChild(combat);
        root->AddChild(pickupSeq);
        root->AddChild(search);
        root->AddChild(patrol);

        Tree = MakeRef<UBehaviorTree>("LeonTournamentBotBehaviorTree");
        Tree->SetRoot(root);
        Tree->SetBlackboardAsset(BoardAsset);
    }

    void ALeonTournamentBotController::PostInitializeComponents() {
        AAIController::PostInitializeComponents();
        if (Perception) {
            FAISightConfig sight;
            sight.SightRadius = 42.0f;
            sight.PeripheralVisionAngleDegrees = 60.0f;
            sight.CloseAwarenessRadius = 20.0f;
            sight.MemoryMaxAge = kLastKnownMemory;
            Perception->SetSightConfig(sight);
            Perception->SetBlackboard(Blackboard.get());
            Perception->SetSensePredicate([this](AActor* actor) {
                auto* other = dynamic_cast<ALeonTournamentCharacter*>(actor);
                auto* self = GetPawn<ALeonTournamentCharacter>();
                if (!other || !self || other == self)
                    return false;
                if (auto* gm = dynamic_cast<ALeonTournamentGameMode*>(World ? World->GetGameMode() : nullptr)) {
                    if (gm->GetActiveGameMode() == ELeonTournamentGameModeId::FreeForAll)
                        return true;
                }
                if (self->GetTeam() != ELeonTournamentTeam::None && other->GetTeam() == self->GetTeam())
                    return false;
                return true;
            });
        }
        BuildBehaviorTree();
        RunBehaviorTree(Tree);
    }

    void ALeonTournamentBotController::DrawDebug() const {
        if (!FGameplayDebugger::ShowAI())
            return;
        auto* pawn = GetPawn<ALeonTournamentCharacter>();
        if (!pawn)
            return;

        const auto& ais = World ? World->GetAIControllers() : std::vector<AAIController*>{};
        int32_t myIndex = 0;
        for (size_t i = 0; i < ais.size(); ++i) {
            if (ais[i] == this)
                myIndex = static_cast<int32_t>(i);
        }
        const bool bSelected = (myIndex == FGameplayDebugger::GetSelectedAIIndex() %
                                               std::max<int32_t>(1, static_cast<int32_t>(ais.size())));

        const glm::vec3 from = pawn->GetActorLocation();
        if (auto* tgt = GetCurrentTarget()) {
            const glm::vec4 c = Blackboard && Blackboard->GetValueAsBool("HasLineOfSight")
                                    ? glm::vec4(1.0f, 0.25f, 0.15f, 1.0f)
                                    : glm::vec4(1.0f, 0.85f, 0.2f, 0.8f);
            FDebugRenderer::DrawDebugLine(from, tgt->GetActorLocation(), c);
        }
        if (PathFollowing && PathFollowing->GetPath().IsValid()) {
            const auto& pts = PathFollowing->GetPath().Points;
            for (size_t i = 1; i < pts.size(); ++i)
                FDebugRenderer::DrawDebugLine(pts[i - 1] + glm::vec3(0, 0.2f, 0), pts[i] + glm::vec3(0, 0.2f, 0),
                                              glm::vec4(0.2f, 0.85f, 1.0f, 0.95f));
            FDebugRenderer::DrawDebugSphere(PathFollowing->GetCurrentDestination(), 0.18f,
                                            glm::vec4(1.0f, 0.9f, 0.2f, 1.0f));
        }

        if (!bSelected)
            return;

        int32_t id = 0;
        if (auto* ps = GetPlayerState())
            id = ps->GetPlayerId();
        char label[256];
        std::snprintf(
            label, sizeof(label), "BOT[%d] %s node=%s tgt=%s los=%s dist=%.1f hp=%.0f ammo=%d path=%s move=%s v=%.1f",
            id, LeonTournamentBotStateName(CachedState), Brain ? Brain->GetActiveNodeName().c_str() : "-",
            GetCurrentTarget() ? GetCurrentTarget()->GetName().c_str() : "-",
            Blackboard && Blackboard->GetValueAsBool("HasLineOfSight") ? "yes" : "no",
            Blackboard ? Blackboard->GetValueAsFloat("DistanceToTarget") : 0.0f,
            pawn->GetHealthComponent() ? pawn->GetHealthComponent()->GetHealth() : 0.0f,
            pawn->GetWeapon() ? pawn->GetWeapon()->GetCurrentAmmo() : 0, NavPathStatusName(GetPathStatus()),
            PathFollowingStatusName(GetMoveStatus()),
            glm::length(pawn->GetCharacterMovement() ? pawn->GetCharacterMovement()->GetVelocity() : glm::vec3(0.0f)));
        PrintString(label, 0.18f, glm::vec4(1.0f, 0.92f, 0.35f, 1.0f), 4000 + id);
        const float radius = Perception ? Perception->GetSightConfig().SightRadius : 42.0f;
        FDebugRenderer::DrawDebugSphere(from, radius, glm::vec4(0.2f, 0.6f, 1.0f, 0.15f), 20);
    }

    void ALeonTournamentBotController::Tick(float DeltaSeconds) {
        AAIController::Tick(DeltaSeconds);
        auto* pawn = GetPawn<ALeonTournamentCharacter>();
        if (!pawn)
            return;
        if (World) {
            if (auto* gs = dynamic_cast<ALeonTournamentGameState*>(World->GetGameState())) {
                if (gs->GetMatchState() != ELeonTournamentMatchState::Playing &&
                    gs->GetMatchState() != ELeonTournamentMatchState::Starting) {
                    pawn->BotSetFireHeld(false);
                    DrawDebug();
                    return;
                }
            }
        }
        DrawDebug();
    }

} // namespace Leon
