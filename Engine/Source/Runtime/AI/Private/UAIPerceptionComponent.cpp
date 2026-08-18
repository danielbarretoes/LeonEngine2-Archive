#include "AI/UAIPerceptionComponent.hpp"
#include "AI/UBlackboardComponent.hpp"
#include "Gameplay/AActor.hpp"
#include "Gameplay/ACharacter.hpp"
#include "Gameplay/AController.hpp"
#include "Gameplay/APawn.hpp"
#include "Gameplay/UHealthComponent.hpp"
#include "Engine/UWorld.hpp"
#include "Physics/FHitResult.hpp"

#include <algorithm>
#include <cmath>

namespace Leon {

    namespace {
        float AngleDegrees(const glm::vec3& A, const glm::vec3& B) {
            const float la = glm::length(A);
            const float lb = glm::length(B);
            if (la < 1e-5f || lb < 1e-5f)
                return 180.0f;
            return glm::degrees(std::acos(std::clamp(glm::dot(A / la, B / lb), -1.0f, 1.0f)));
        }

        float PlanarDistance(const glm::vec3& A, const glm::vec3& B) {
            glm::vec3 d = A - B;
            d.y = 0.0f;
            return glm::length(d);
        }
    } // namespace

    UAIPerceptionComponent::UAIPerceptionComponent(const std::string& InName) : UActorComponent(InName) {}

    void UAIPerceptionComponent::Tick(float DeltaSeconds) {
        AgeMemory(DeltaSeconds);
        UpdateAccumulator += DeltaSeconds;
        if (UpdateAccumulator < SightConfig.UpdateInterval)
            return;
        UpdateAccumulator = 0.0f;
        UpdatePerception();
    }

    void UAIPerceptionComponent::AgeMemory(float DeltaSeconds) {
        LastKnownAge += DeltaSeconds;
        if (LastKnownAge >= SightConfig.MemoryMaxAge)
            LastKnownAge = SightConfig.MemoryMaxAge;
    }

    APawn* UAIPerceptionComponent::GetSensePawn() const {
        if (auto* controller = dynamic_cast<AController*>(GetOwner()))
            return controller->GetPawn();
        return dynamic_cast<APawn*>(GetOwner());
    }

    glm::vec3 UAIPerceptionComponent::GetSenseOrigin() const {
        APawn* pawn = GetSensePawn();
        if (!pawn)
            return glm::vec3(0.0f);
        glm::vec3 origin = pawn->GetActorLocation();
        origin.y += SightConfig.EyeHeightOffset;
        return origin;
    }

    glm::vec3 UAIPerceptionComponent::GetSenseForward() const {
        APawn* pawn = GetSensePawn();
        if (!pawn)
            return glm::vec3(0.0f, 0.0f, -1.0f);
        if (auto* character = dynamic_cast<ACharacter*>(pawn))
            return character->GetControlLookDirection();
        return pawn->GetActorForwardVector();
    }

    bool UAIPerceptionComponent::IsActorDead(AActor* InActor) {
        if (!InActor)
            return true;
        if (auto* health = InActor->FindComponentByClass<UHealthComponent>())
            return health->IsDead();
        return false;
    }

    bool UAIPerceptionComponent::PassesPredicate(AActor* InActor) const {
        APawn* self = GetSensePawn();
        if (!InActor || InActor->IsPendingKill() || InActor == self)
            return false;
        if (IsActorDead(InActor))
            return false;
        if (SensePredicate)
            return SensePredicate(InActor);
        return dynamic_cast<APawn*>(InActor) != nullptr;
    }

    bool UAIPerceptionComponent::HasLineOfSight(AActor& InTarget) const {
        APawn* self = GetSensePawn();
        UWorld* world = self ? self->GetWorld() : (GetOwner() ? GetOwner()->GetWorld() : nullptr);
        if (!self || !world)
            return false;
        glm::vec3 origin = GetSenseOrigin();
        glm::vec3 chest = InTarget.GetActorLocation();
        glm::vec3 to = chest - origin;
        if (glm::length(to) < 0.15f)
            return true;
        FHitResult hit;
        if (!world->LineTraceSingleByChannel(origin, chest, SightConfig.TraceChannel, self, hit))
            return true;
        return hit.Actor == &InTarget;
    }

    bool UAIPerceptionComponent::IsInSightCone(AActor& InTarget) const {
        APawn* self = GetSensePawn();
        if (!self)
            return false;
        const glm::vec3 origin = self->GetActorLocation();
        glm::vec3 to = InTarget.GetActorLocation() - origin;
        const float dist = glm::length(to);
        if (dist > SightConfig.SightRadius)
            return false;
        if (dist <= SightConfig.CloseAwarenessRadius)
            return true;
        glm::vec3 planar = to;
        planar.y = 0.0f;
        const glm::vec3 forward = GetSenseForward();
        const float ang = AngleDegrees(glm::vec3(forward.x, 0.0f, forward.z), planar);
        return ang <= SightConfig.PeripheralVisionAngleDegrees;
    }

    float UAIPerceptionComponent::ScoreCandidate(AActor& InActor, AActor* InPrevious) const {
        APawn* self = GetSensePawn();
        if (!self)
            return -1.0e9f;
        glm::vec3 to = InActor.GetActorLocation() - self->GetActorLocation();
        const float dist = glm::length(to);
        glm::vec3 planar = to;
        planar.y = 0.0f;
        const glm::vec3 forward = GetSenseForward();
        const float ang = AngleDegrees(glm::vec3(forward.x, 0.0f, forward.z), planar);
        float score = (1.0f - dist / std::max(SightConfig.SightRadius, 0.01f)) * 40.0f +
                      (1.0f - ang / 90.0f) * 20.0f;
        if (HasLineOfSight(InActor))
            score += 100.0f;
        if (&InActor == InPrevious)
            score += 25.0f;
        return score;
    }

    bool UAIPerceptionComponent::HasLastKnown() const {
        return LastKnownAge < SightConfig.MemoryMaxAge && glm::length(LastKnownLocation) > 0.01f;
    }

    void UAIPerceptionComponent::RememberActor(AActor* InActor) {
        if (!InActor)
            return;
        LastKnownLocation = InActor->GetActorLocation();
        LastKnownAge = 0.0f;
        CurrentTarget = InActor;
    }

    void UAIPerceptionComponent::ForgetLastKnown() {
        LastKnownLocation = glm::vec3(0.0f);
        LastKnownAge = SightConfig.MemoryMaxAge;
        CurrentTarget = nullptr;
        PerceivedActors.clear();
        WriteToBlackboard();
    }

    void UAIPerceptionComponent::WriteToBlackboard() {
        if (!Blackboard)
            return;
        Blackboard->SetValueAsObject("TargetActor", CurrentTarget);
        Blackboard->SetValueAsBool("HasTarget", CurrentTarget != nullptr);
        if (CurrentTarget) {
            Blackboard->SetValueAsVector("TargetLocation", CurrentTarget->GetActorLocation());
            const bool bLos = HasLineOfSight(*CurrentTarget);
            Blackboard->SetValueAsBool("HasLineOfSight", bLos);
            APawn* self = GetSensePawn();
            Blackboard->SetValueAsFloat("DistanceToTarget",
                                        self ? PlanarDistance(self->GetActorLocation(), CurrentTarget->GetActorLocation())
                                             : 0.0f);
        } else {
            Blackboard->SetValueAsBool("HasLineOfSight", false);
            Blackboard->SetValueAsFloat("DistanceToTarget", 0.0f);
        }
        Blackboard->SetValueAsBool("HasLastKnown", HasLastKnown());
        Blackboard->SetValueAsVector("LastKnownLocation", LastKnownLocation);
        Blackboard->SetValueAsVector("LastKnownTargetLocation", LastKnownLocation);
    }

    void UAIPerceptionComponent::UpdatePerception() {
        PerceivedActors.clear();
        APawn* self = GetSensePawn();
        UWorld* world = self ? self->GetWorld() : (GetOwner() ? GetOwner()->GetWorld() : nullptr);
        if (!self || !world) {
            CurrentTarget = nullptr;
            WriteToBlackboard();
            return;
        }

        AActor* previous = CurrentTarget;
        if (previous && (previous->IsPendingKill() || IsActorDead(previous) || !PassesPredicate(previous)))
            previous = nullptr;

        AActor* best = nullptr;
        float bestScore = -1.0e9f;
        for (const auto& actor : world->GetAllActors()) {
            AActor* other = actor.get();
            if (!PassesPredicate(other) || !IsInSightCone(*other))
                continue;
            PerceivedActors.push_back(other);
            const float score = ScoreCandidate(*other, previous);
            if (score > bestScore) {
                bestScore = score;
                best = other;
            }
        }

        CurrentTarget = best;
        if (CurrentTarget && HasLineOfSight(*CurrentTarget)) {
            LastKnownLocation = CurrentTarget->GetActorLocation();
            LastKnownAge = 0.0f;
        }
        WriteToBlackboard();
    }

} // namespace Leon
