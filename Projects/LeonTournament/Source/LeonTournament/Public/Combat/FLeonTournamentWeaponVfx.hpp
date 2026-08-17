#pragma once

#include "FLeonTournamentTypes.hpp"

#include <functional>
#include <glm/glm.hpp>

namespace Leon {

    class UWorld;
    class ALeonTournamentCharacter;

    struct FLeonTournamentWeaponVfxContext {
        UWorld* World = nullptr;
        ALeonTournamentCharacter* OwnerCharacter = nullptr;
        ELeonTournamentWeaponId WeaponId = ELeonTournamentWeaponId::Rifle;
        const FLeonTournamentWeaponConfig* Config = nullptr;
    };

    /** Muzzle / tracer / impact FX. Returns emitter spawn count for HUD / debug. */
    struct FLeonTournamentWeaponVfx {
        static int SpawnFireEffects(const FLeonTournamentWeaponVfxContext& Ctx, const glm::vec3& InMuzzle,
                                    const glm::vec3& InTracerStart, const glm::vec3& InTraceEnd, bool bHitWorld,
                                    bool bHitCharacter);

        static int SpawnRocketLaunchEffects(const FLeonTournamentWeaponVfxContext& Ctx, const glm::vec3& InMuzzle);

        static int SpawnLaserEffects(const FLeonTournamentWeaponVfxContext& Ctx, const glm::vec3& InMuzzle,
                                     const glm::vec3& InTraceEnd, bool bHitCharacter);

        static int SpawnShotgunBlastEffects(const FLeonTournamentWeaponVfxContext& Ctx, const glm::vec3& InMuzzle,
                                            const glm::vec3& InAimDir, float InCurrentSpreadDeg,
                                            const std::function<glm::vec3(const glm::vec3&, float)>& ApplySpread);

        static int SpawnFlameEffects(const FLeonTournamentWeaponVfxContext& Ctx, const glm::vec3& InMuzzle,
                                     const glm::vec3& InDir);
    };

} // namespace Leon
