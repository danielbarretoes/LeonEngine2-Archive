#pragma once

#include "FLeonTournamentTypes.hpp"

namespace Leon {

    class ALeonTournamentCharacter;

    /** Pure match-config damage eligibility (friendly fire / self / dead). */
    struct FLeonTournamentDamageRules {
        static bool CanDamage(const FLeonTournamentMatchConfig& Config, const ALeonTournamentCharacter& Instigator,
                              const ALeonTournamentCharacter& Target);
    };

} // namespace Leon
