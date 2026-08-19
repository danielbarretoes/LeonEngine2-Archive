#pragma once

#include "FLeonTournamentTypes.hpp"
#include "Gameplay/FDamageInfo.hpp"

#include <glm/glm.hpp>

namespace Leon {

    class ALeonTournamentCharacter;

    /** Pure match-config damage eligibility (friendly fire / self / dead). */
    struct FLeonTournamentDamageRules {
        static bool CanDamage(const FLeonTournamentMatchConfig& Config, const ALeonTournamentCharacter& Instigator,
                              const ALeonTournamentCharacter& Target);

        /** True when InHitLocation is in the head band (eye height to capsule top). */
        static bool IsHeadHit(const ALeonTournamentCharacter& InTarget, const glm::vec3& InHitLocation);

        /** Point damage in the head band is multiplied and marked as a critical hit. */
        static void ApplyHeadshotIfHit(const ALeonTournamentCharacter& InTarget, FDamageInfo& InOutInfo);
    };

} // namespace Leon
