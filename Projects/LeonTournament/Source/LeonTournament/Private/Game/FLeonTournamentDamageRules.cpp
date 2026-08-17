#include "FLeonTournamentDamageRules.hpp"
#include "ALeonTournamentCharacter.hpp"
#include "Gameplay/UHealthComponent.hpp"

namespace Leon {

    bool FLeonTournamentDamageRules::CanDamage(const FLeonTournamentMatchConfig& Config,
                                               const ALeonTournamentCharacter& Instigator,
                                               const ALeonTournamentCharacter& Target) {
        if (Target.GetHealthComponent() && Target.GetHealthComponent()->IsDead())
            return false;
        // Self-damage allowed (rocket jumps / splash). Friendly fire still blocked for allies.
        if (&Instigator == &Target)
            return true;
        if (!Config.bFriendlyFire && Instigator.GetTeam() != ELeonTournamentTeam::None &&
            Instigator.GetTeam() == Target.GetTeam())
            return false;
        return true;
    }

} // namespace Leon
