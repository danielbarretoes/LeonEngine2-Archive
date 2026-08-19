#include "FLeonTournamentDamageRules.hpp"
#include "ALeonTournamentCharacter.hpp"
#include "Gameplay/UHealthComponent.hpp"

namespace Leon {

    bool FLeonTournamentDamageRules::CanDamage(const FLeonTournamentMatchConfig& Config,
                                               const ALeonTournamentCharacter& Instigator,
                                               const ALeonTournamentCharacter& Target) {
        if (Target.GetHealthComponent() && Target.GetHealthComponent()->IsDead())
            return false;
        if (Target.IsSpawnProtected())
            return false;
        // Self-damage allowed (rocket jumps / splash). Friendly fire still blocked for allies.
        if (&Instigator == &Target)
            return true;
        if (!Config.bFriendlyFire && Instigator.GetTeam() != ELeonTournamentTeam::None &&
            Instigator.GetTeam() == Target.GetTeam())
            return false;
        return true;
    }

    bool FLeonTournamentDamageRules::IsHeadHit(const ALeonTournamentCharacter& InTarget,
                                               const glm::vec3& InHitLocation) {
        const glm::vec3 origin = InTarget.GetActorLocation();
        const float halfH = InTarget.GetCapsuleHalfHeight();
        const float topY = origin.y + halfH;
        const float chinY = InTarget.GetPawnViewLocation().y - kLeonTournamentHeadshotChinBelowEyeMeters;
        if (InHitLocation.y < chinY || InHitLocation.y > topY + 0.05f)
            return false;
        const float r = InTarget.GetCapsuleRadius() + 0.08f;
        const float dx = InHitLocation.x - origin.x;
        const float dz = InHitLocation.z - origin.z;
        return (dx * dx + dz * dz) <= r * r;
    }

    void FLeonTournamentDamageRules::ApplyHeadshotIfHit(const ALeonTournamentCharacter& InTarget,
                                                        FDamageInfo& InOutInfo) {
        if (InOutInfo.DamageType == EDamageType::Radial)
            return;
        if (!IsHeadHit(InTarget, InOutInfo.HitLocation))
            return;
        InOutInfo.DamageAmount *= kLeonTournamentHeadshotMultiplier;
        InOutInfo.bCriticalHit = true;
    }

} // namespace Leon
