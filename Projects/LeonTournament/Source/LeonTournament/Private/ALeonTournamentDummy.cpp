#include "ALeonTournamentDummy.hpp"
#include "ALeonTournamentGameMode.hpp"
#include "Engine/UWorld.hpp"
#include "Gameplay/FDamageInfo.hpp"

#include <cmath>
#include <glm/glm.hpp>

namespace Leon {

    ALeonTournamentDummy::ALeonTournamentDummy(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : ALeonTournamentCharacter(InHandle, InWorld, InName) {
        SetClass("ALeonTournamentDummy");
    }

    void ALeonTournamentDummy::PostInitializeComponents() {
        ALeonTournamentCharacter::PostInitializeComponents();
        SetThirdPerson(true);
        SetBotControlled(true);
    }

    void ALeonTournamentDummy::BeginPlay() {
        ALeonTournamentCharacter::BeginPlay();
        if (auto combat = GetCombatComponent(); combat && !bCombatBound) {
            combat->SetAttackCooldown(1.25f);
            combat->OnAttack.push_back([this]() { PunchNearest(); });
            bCombatBound = true;
        }
    }

    void ALeonTournamentDummy::Tick(float DeltaSeconds) {
        ALeonTournamentCharacter::Tick(DeltaSeconds);
        if (IsDeadFrozen())
            return;
        ALeonTournamentCharacter* nearest = nullptr;
        float best = 1.0e9f;
        if (World) {
            for (const auto& actor : World->GetAllActors()) {
                auto* ch = dynamic_cast<ALeonTournamentCharacter*>(actor.get());
                if (!ch || ch == this || ch->IsPendingKill() || ch->IsDeadFrozen())
                    continue;
                const float d = glm::length(ch->GetActorLocation() - GetActorLocation());
                if (d < best) {
                    best = d;
                    nearest = ch;
                }
            }
        }
        if (nearest)
            BotLookAt(nearest->GetActorLocation());
        if (auto combat = GetCombatComponent())
            combat->Attack();
        (void)DeltaSeconds;
    }

    void ALeonTournamentDummy::PunchNearest() {
        if (!World || IsDeadFrozen())
            return;
        ALeonTournamentCharacter* nearest = nullptr;
        float best = 2.4f;
        for (const auto& actor : World->GetAllActors()) {
            auto* ch = dynamic_cast<ALeonTournamentCharacter*>(actor.get());
            if (!ch || ch == this || ch->IsPendingKill() || ch->IsDeadFrozen())
                continue;
            const float d = glm::length(ch->GetActorLocation() - GetActorLocation());
            if (d < best) {
                best = d;
                nearest = ch;
            }
        }
        if (!nearest)
            return;
        FDamageInfo info;
        info.DamageAmount = 8.0f;
        info.DamageType = EDamageType::Point;
        info.Instigator = this;
        info.Causer = this;
        info.HitActor = nearest;
        info.HitLocation = nearest->GetActorLocation();
        info.Impulse = glm::normalize(nearest->GetActorLocation() - GetActorLocation() + glm::vec3(0.0f, 0.2f, 0.0f)) *
                       2.0f;
        if (auto* gm = dynamic_cast<ALeonTournamentGameMode*>(World->GetGameMode()))
            gm->ApplyAuthoritativeDamage(*this, *nearest, info);
        else
            nearest->ApplyDamageFrom(info);
    }

} // namespace Leon
