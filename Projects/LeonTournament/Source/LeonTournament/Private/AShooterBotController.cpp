#include "AShooterBotController.hpp"
#include "AShooterCharacter.hpp"
#include "AShooterPlayerState.hpp"
#include "AShooterWeapon.hpp"
#include "AShooterGameState.hpp"
#include "AShooterGameMode.hpp"
#include "Engine/UWorld.hpp"
#include "Renderer/FDebugRenderer.hpp"
#include "Gameplay/UGameplayStatics.hpp"
#include "Gameplay/FGameplayDebugger.hpp"
#include "Gameplay/UCharacterMovementComponent.hpp"
#include "AI/UBehaviorTree.hpp"
#include "AI/UPathFollowingComponent.hpp"
#include "Core/FWorldUnits.hpp"
#include "Core/FInput.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace Leon {

    namespace {
        constexpr float kDetectionRadius = 35.0f;
        constexpr float kAwarenessRadius = 12.0f;
        constexpr float kFovDegrees = 120.0f;
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

    AShooterBotController::AShooterBotController(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AAIController(InHandle, InWorld, InName) {
        SetClass("AShooterBotController");
    }

    AShooterCharacter* AShooterBotController::GetShooterPawn() const { return GetPawn<AShooterCharacter>(); }

    AShooterCharacter* AShooterBotController::GetCurrentTarget() const {
        if (!Blackboard)
            return nullptr;
        return static_cast<AShooterCharacter*>(Blackboard->GetValueAsObject("TargetActor"));
    }

    EShooterBotState AShooterBotController::GetBotState() const { return CachedState; }

    bool AShooterBotController::HasLineOfSight(AShooterCharacter& InTarget) const {
        auto* self = GetShooterPawn();
        if (!self || !World)
            return false;
        glm::vec3 origin = self->GetActorLocation();
        origin.y += 0.25f;
        glm::vec3 chest = InTarget.GetActorLocation();
        glm::vec3 to = chest - origin;
        if (glm::length(to) < 0.15f)
            return true;
        FHitResult hit;
        if (!World->LineTraceSingleByChannel(origin, chest, ECollisionChannel::Visibility, self, hit))
            return true;
        return hit.Actor == &InTarget;
    }

    void AShooterBotController::AssignPersonality() {
        int32_t id = 1;
        if (auto* ps = GetPlayerState())
            id = std::max(1, ps->GetPlayerId());
        Personality.Aggression = 0.35f + static_cast<float>(id % 5) * 0.12f;
        Personality.Accuracy = 0.45f + static_cast<float>((id * 3) % 5) * 0.1f;
        Personality.ReactionTime = 0.15f + static_cast<float>((id * 7) % 4) * 0.05f;
        Personality.PreferredRange = 10.0f + static_cast<float>((id * 11) % 6) * 2.5f;
        Personality.StrafeFrequency = 0.7f + static_cast<float>((id * 13) % 5) * 0.25f;
        Personality.CoverPreference = 0.25f + static_cast<float>((id * 17) % 5) * 0.14f;
        Personality.Tactic = id % 3;
    }

    void AShooterBotController::Possess(APawn* InPawn) {
        AAIController::Possess(InPawn);
        AssignPersonality();
        auto* ch = GetShooterPawn();
        if (!ch || !ch->GetHealthComponent() || bDamageBound)
            return;
        ch->GetHealthComponent()->OnDamage.push_back([this](const FDamageInfo& info) { NotifyDamaged(info); });
        bDamageBound = true;
    }

    void AShooterBotController::NotifyDamaged(const FDamageInfo& InInfo) {
        auto* attacker = dynamic_cast<AShooterCharacter*>(InInfo.Instigator);
        auto* self = GetShooterPawn();
        if (!attacker || !self || attacker == self || !Blackboard)
            return;
        if (self->GetTeam() != EShooterTeam::None && attacker->GetTeam() == self->GetTeam())
            return;
        Blackboard->SetValueAsObject("TargetActor", attacker);
        Blackboard->SetValueAsBool("HasTarget", true);
        Blackboard->SetValueAsVector("LastKnownTargetLocation", attacker->GetActorLocation());
        LastKnownLocation = attacker->GetActorLocation();
        LastKnownAge = 0.0f;
        AcquireTime = 0.0f;
    }

    void AShooterBotController::NotifyRespawned() {
        LastKnownAge = kLastKnownMemory;
        AcquireTime = 0.0f;
        StrafeTimer = 0.0f;
        LookAroundTimer = 0.0f;
        CachedState = EShooterBotState::Respawn;
        if (Blackboard) {
            Blackboard->SetValueAsBool("IsDead", false);
            Blackboard->SetValueAsObject("TargetActor", nullptr);
            Blackboard->SetValueAsBool("HasTarget", false);
            Blackboard->SetValueAsBool("HasLineOfSight", false);
            Blackboard->SetValueAsBool("IsLowHealth", false);
            Blackboard->SetValueAsBool("HasAmmo", true);
            Blackboard->SetValueAsBool("IsReloading", false);
        }
        if (Brain && Tree) {
            if (!Brain->IsRunning())
                Brain->StartTree(Tree);
        }
        StopMovement();
    }

    bool AShooterBotController::IsBehaviorTreeRunning() const { return Brain && Brain->IsRunning(); }

    glm::vec3 AShooterBotController::PickApproachLocation(const glm::vec3& InFrom, const glm::vec3& InTarget,
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

    glm::vec3 AShooterBotController::PickCoverLocation(AShooterCharacter& InSelf, AShooterCharacter* InTarget) const {
        glm::vec3 best = InSelf.GetActorLocation();
        float bestScore = -1e9f;
        for (const glm::vec3& p : CoverPoints) {
            const float d = PlanarDistance(InSelf.GetActorLocation(), p);
            if (d > 22.0f)
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

    glm::vec3 AShooterBotController::PickPatrolLocation(AShooterCharacter& InSelf) const {
        if (Waypoints.empty())
            return InSelf.GetActorLocation() + glm::vec3(4.0f, 0.0f, 0.0f);
        const size_t n = Waypoints.size();
        size_t idx = static_cast<size_t>(rand()) % n;
        if (Personality.Tactic == 2)
            idx = std::min(idx, n / 2);
        else if (Personality.Tactic == 0)
            idx = n / 2 + (idx % std::max<size_t>(1, n / 2));
        return Waypoints[idx];
    }

    void AShooterBotController::TickAim(float DeltaSeconds, const glm::vec3& InWorldPoint) {
        auto* pawn = GetShooterPawn();
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

    bool AShooterBotController::IsAimAligned(const glm::vec3& InWorldPoint, float InDegrees) const {
        auto* pawn = GetShooterPawn();
        if (!pawn)
            return false;
        glm::vec3 to = InWorldPoint - pawn->GetActorLocation();
        if (glm::length(to) < 1e-4f)
            return true;
        return AngleDegrees(pawn->GetControlLookDirection(), to) <= InDegrees;
    }

    void AShooterBotController::TickPerception() {
        auto board = Blackboard;
        auto* self = GetShooterPawn();
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

        AShooterCharacter* best = GetCurrentTarget();
        if (best && (best->IsPendingKill() || (best->GetHealthComponent() && best->GetHealthComponent()->IsDead())))
            best = nullptr;

        float bestScore = -1e9f;
        const glm::vec3 origin = self->GetActorLocation();
        const glm::vec3 forward = self->GetControlLookDirection();
        AShooterCharacter* prev = best;

        for (const auto& actor : World->GetAllActors()) {
            auto* other = dynamic_cast<AShooterCharacter*>(actor.get());
            if (!other || other == self || other->IsPendingKill())
                continue;
            if (other->GetHealthComponent() && other->GetHealthComponent()->IsDead())
                continue;
            if (self->GetTeam() != EShooterTeam::None && other->GetTeam() == self->GetTeam())
                continue;
            glm::vec3 to = other->GetActorLocation() - origin;
            const float dist = glm::length(to);
            if (dist > kDetectionRadius)
                continue;
            glm::vec3 planar = to;
            planar.y = 0.0f;
            const float ang = AngleDegrees(glm::vec3(forward.x, 0.0f, forward.z), planar);
            const bool bInFov = ang <= kFovDegrees * 0.5f || dist <= kAwarenessRadius;
            if (!bInFov)
                continue;
            const bool bLos = HasLineOfSight(*other);
            float score = (1.0f - dist / kDetectionRadius) * 40.0f + (1.0f - ang / 90.0f) * 20.0f;
            if (bLos)
                score += 100.0f;
            if (other == prev)
                score += 25.0f;
            if (score > bestScore) {
                bestScore = score;
                best = other;
            }
        }

        board->SetValueAsObject("TargetActor", best);
        board->SetValueAsBool("HasTarget", best != nullptr);
        if (best) {
            board->SetValueAsVector("TargetLocation", best->GetActorLocation());
            const bool bLos = HasLineOfSight(*best);
            board->SetValueAsBool("HasLineOfSight", bLos);
            board->SetValueAsFloat("DistanceToTarget", PlanarDistance(origin, best->GetActorLocation()));
            if (bLos) {
                LastKnownLocation = best->GetActorLocation();
                LastKnownAge = 0.0f;
                board->SetValueAsVector("LastKnownTargetLocation", LastKnownLocation);
            }
        } else {
            board->SetValueAsBool("HasLineOfSight", false);
            board->SetValueAsFloat("DistanceToTarget", 0.0f);
        }
        board->SetValueAsBool("HasLastKnown", LastKnownAge < kLastKnownMemory && glm::length(LastKnownLocation) > 0.01f);
    }

    void AShooterBotController::BuildBehaviorTree() {
        BoardAsset = MakeRef<UBlackboardData>("ShooterBotBB");
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

        auto perception = MakeRef<UBTService_Native>("Perception", [this](UBehaviorTreeComponent&, float) {
            TickPerception();
        });
        perception->Interval = 0.12f;
        perception->TimeAccumulator = perception->Interval;

        auto dead = MakeRef<UBTTask_Native>("Dead", [this](UBehaviorTreeComponent& owner, float) {
            auto board = owner.GetBlackboard();
            auto* pawn = GetShooterPawn();
            const bool bDead =
                (pawn && pawn->GetHealthComponent() && pawn->GetHealthComponent()->IsDead()) ||
                (board && board->GetValueAsBool("IsDead"));
            if (!bDead)
                return EBTNodeResult::Failed;
            if (board)
                board->SetValueAsBool("IsDead", true);
            CachedState = EShooterBotState::Dead;
            if (pawn)
                pawn->BotSetFireHeld(false);
            StopMovement();
            return EBTNodeResult::Succeeded;
        });

        auto reload = MakeRef<UBTTask_Native>("Reload", [this](UBehaviorTreeComponent&, float) {
            auto* pawn = GetShooterPawn();
            if (!pawn)
                return EBTNodeResult::Failed;
            CachedState = EShooterBotState::Reload;
            pawn->BotSetFireHeld(false);
            pawn->BotRequestReload();
            return pawn->GetWeapon() && pawn->GetWeapon()->IsReloading() ? EBTNodeResult::InProgress
                                                                        : EBTNodeResult::Succeeded;
        });

        auto cover = MakeRef<UBTTask_Native>("TakeCover", [this](UBehaviorTreeComponent& owner, float dt) {
            auto board = owner.GetBlackboard();
            auto* pawn = GetShooterPawn();
            if (!board || !pawn || !board->GetValueAsBool("IsLowHealth"))
                return EBTNodeResult::Failed;
            if (CoverPoints.empty() || Personality.CoverPreference < 0.35f)
                return EBTNodeResult::Failed;
            CachedState = EShooterBotState::TakeCover;
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
            auto* pawn = GetShooterPawn();
            auto* target = GetCurrentTarget();
            if (!board || !pawn || !target || !board->GetValueAsBool("HasTarget")) {
                if (pawn)
                    pawn->BotSetFireHeld(false);
                return EBTNodeResult::Failed;
            }
            CachedState = EShooterBotState::Combat;
            AcquireTime += dt;
            StrafeTimer -= dt;
            const glm::vec3 tgt = target->GetActorLocation();
            const float dist = PlanarDistance(pawn->GetActorLocation(), tgt);
            const float ideal = Personality.PreferredRange;
            TickAim(dt, tgt);

            if (dist > kMaxRange || dist > ideal + 6.0f) {
                pawn->BotSetFireHeld(false);
                CachedState = EShooterBotState::MoveToTarget;
                const glm::vec3 dest = PickApproachLocation(pawn->GetActorLocation(), tgt, ideal);
                board->SetValueAsVector("DesiredLocation", dest);
                MoveToLocation(dest, 1.0f);
                return EBTNodeResult::InProgress;
            }
            if (dist < kMinRange) {
                pawn->BotSetFireHeld(false);
                CachedState = EShooterBotState::MoveToTarget;
                const glm::vec3 dest = PickApproachLocation(pawn->GetActorLocation(), tgt, ideal);
                MoveToLocation(dest, 1.0f);
                return EBTNodeResult::InProgress;
            }

            StopMovement();

            if (StrafeTimer <= 0.0f) {
                StrafeSign = (rand() % 2) ? 1.0f : -1.0f;
                StrafeTimer = Personality.StrafeFrequency;
            }
            glm::vec3 to = tgt - pawn->GetActorLocation();
            to.y = 0.0f;
            if (glm::length(to) > 1e-4f)
                to = glm::normalize(to);
            glm::vec3 right(-to.z, 0.0f, to.x);
            pawn->AddMovementInput(right, StrafeSign * pawn->GetMoveSpeed() * 0.45f);

            const bool bLos = board->GetValueAsBool("HasLineOfSight");
            const float align = 12.0f + (1.0f - Personality.Accuracy) * 10.0f;
            const bool bReady = AcquireTime >= Personality.ReactionTime && bLos && IsAimAligned(tgt, align);
            pawn->BotSetFireHeld(bReady);
            if (bReady)
                CachedState = EShooterBotState::Fire;
            return EBTNodeResult::InProgress;
        });

        auto search = MakeRef<UBTTask_Native>("Search", [this](UBehaviorTreeComponent& owner, float dt) {
            auto board = owner.GetBlackboard();
            auto* pawn = GetShooterPawn();
            if (!board || !pawn || !board->GetValueAsBool("HasLastKnown"))
                return EBTNodeResult::Failed;
            CachedState = EShooterBotState::Search;
            pawn->BotSetFireHeld(false);
            const glm::vec3 dest = board->GetValueAsVector("LastKnownTargetLocation");
            MoveToLocation(dest, 1.0f);
            LookAroundTimer += dt;
            TickAim(dt, dest + glm::vec3(std::sin(LookAroundTimer * 2.0f) * 2.0f, 0.0f,
                                         std::cos(LookAroundTimer * 1.7f) * 2.0f));
            if (PlanarDistance(pawn->GetActorLocation(), dest) < 1.2f && LookAroundTimer > 1.6f) {
                LastKnownAge = kLastKnownMemory;
                board->SetValueAsBool("HasLastKnown", false);
                return EBTNodeResult::Succeeded;
            }
            return EBTNodeResult::InProgress;
        });

        auto patrol = MakeRef<UBTTask_Native>("Patrol", [this](UBehaviorTreeComponent& owner, float dt) {
            auto* pawn = GetShooterPawn();
            auto board = owner.GetBlackboard();
            if (!pawn || !board)
                return EBTNodeResult::Failed;
            CachedState = EShooterBotState::Idle;
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

        auto root = MakeRef<UBTComposite_Selector>("Selector");
        root->Services.push_back(perception);
        root->AddChild(dead);
        root->AddChild(reloadSeq);
        root->AddChild(coverSeq);
        root->AddChild(combat);
        root->AddChild(search);
        root->AddChild(patrol);

        Tree = MakeRef<UBehaviorTree>("ShooterBotBehaviorTree");
        Tree->SetRoot(root);
        Tree->SetBlackboardAsset(BoardAsset);
    }

    void AShooterBotController::PostInitializeComponents() {
        AAIController::PostInitializeComponents();
        BuildBehaviorTree();
        RunBehaviorTree(Tree);
    }

    void AShooterBotController::DrawDebug() const {
        if (!FGameplayDebugger::ShowAI())
            return;
        auto* pawn = GetShooterPawn();
        if (!pawn)
            return;

        const auto& ais = World ? World->GetAIControllers() : std::vector<AAIController*>{};
        int32_t myIndex = 0;
        for (size_t i = 0; i < ais.size(); ++i) {
            if (ais[i] == this)
                myIndex = static_cast<int32_t>(i);
        }
        const bool bSelected = (myIndex == FGameplayDebugger::GetSelectedAIIndex() % std::max<int32_t>(1, static_cast<int32_t>(ais.size())));

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
        std::snprintf(label, sizeof(label),
                      "BOT[%d] %s node=%s tgt=%s los=%s dist=%.1f hp=%.0f ammo=%d path=%s move=%s v=%.1f", id,
                      ShooterBotStateName(CachedState), Brain ? Brain->GetActiveNodeName().c_str() : "-",
                      GetCurrentTarget() ? GetCurrentTarget()->GetName().c_str() : "-",
                      Blackboard && Blackboard->GetValueAsBool("HasLineOfSight") ? "yes" : "no",
                      Blackboard ? Blackboard->GetValueAsFloat("DistanceToTarget") : 0.0f,
                      pawn->GetHealthComponent() ? pawn->GetHealthComponent()->GetHealth() : 0.0f,
                      pawn->GetWeapon() ? pawn->GetWeapon()->GetCurrentAmmo() : 0, NavPathStatusName(GetPathStatus()),
                      PathFollowingStatusName(GetMoveStatus()),
                      glm::length(pawn->GetCharacterMovement() ? pawn->GetCharacterMovement()->GetVelocity()
                                                               : glm::vec3(0.0f)));
        PrintString(label, 0.18f, glm::vec4(1.0f, 0.92f, 0.35f, 1.0f), 4000 + id);
        FDebugRenderer::DrawDebugSphere(from, kDetectionRadius, glm::vec4(0.2f, 0.6f, 1.0f, 0.15f), 20);
    }

    void AShooterBotController::Tick(float DeltaSeconds) {
        LastKnownAge += DeltaSeconds;
        AAIController::Tick(DeltaSeconds);
        auto* pawn = GetShooterPawn();
        if (!pawn)
            return;
        if (World) {
            if (auto* gs = dynamic_cast<AShooterGameState*>(World->GetGameState())) {
                if (gs->GetMatchState() != EShooterMatchState::Playing &&
                    gs->GetMatchState() != EShooterMatchState::Starting) {
                    pawn->BotSetFireHeld(false);
                    DrawDebug();
                    return;
                }
            }
        }
        DrawDebug();
    }

} // namespace Leon
