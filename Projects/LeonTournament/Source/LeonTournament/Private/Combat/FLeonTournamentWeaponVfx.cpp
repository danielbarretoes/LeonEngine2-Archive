#include "FLeonTournamentWeaponVfx.hpp"
#include "ALeonTournamentCharacter.hpp"
#include "Gameplay/UGameplayStatics.hpp"
#include "Gameplay/UParticleComponent.hpp"

#include <algorithm>
#include <cmath>

namespace Leon {

    int FLeonTournamentWeaponVfx::SpawnFireEffects(const FLeonTournamentWeaponVfxContext& Ctx, const glm::vec3& InMuzzle,
                         const glm::vec3& InTracerStart, const glm::vec3& InTraceEnd, bool bHitWorld,
                         bool bHitCharacter) {
        int spawnCount = 0;
        if (!Ctx.World)
            return spawnCount;

        FParticleEmitterSettings muzzle;
        muzzle.Kind = EParticleKind::SpriteBurst;
        muzzle.BurstCount = Ctx.WeaponId == ELeonTournamentWeaponId::Shotgun ? 16 : 10;
        muzzle.Lifetime = 0.07f;
        muzzle.Size = 0.05f;
        muzzle.SizeEnd = 0.12f;
        muzzle.Color = {1.0f, 0.82f, 0.35f, 1.0f};
        muzzle.ColorEnd = {1.0f, 0.4f, 0.05f, 0.0f};
        muzzle.VelocityMin = {-0.15f, -0.05f, -0.15f};
        muzzle.VelocityMax = {0.15f, 0.25f, 0.15f};
        muzzle.Gravity = {0.0f, 0.0f, 0.0f};
        if (UGameplayStatics::SpawnEmitterAtLocation(Ctx.World, muzzle, InMuzzle))
            ++spawnCount;

        FParticleEmitterSettings tracer;
        tracer.Kind = EParticleKind::Beam;
        tracer.Lifetime = 0.055f;
        tracer.Color = {1.0f, 0.88f, 0.4f, 0.75f};
        tracer.ColorEnd = {1.0f, 0.55f, 0.15f, 0.0f};
        tracer.BeamEnd = InTraceEnd;
        tracer.BeamThickness = Ctx.WeaponId == ELeonTournamentWeaponId::Shotgun ? 0.01f : 0.014f;
        if (UGameplayStatics::SpawnEmitterAtLocation(Ctx.World, tracer, InTracerStart))
            ++spawnCount;

        if (bHitWorld || bHitCharacter) {
            FParticleEmitterSettings impact;
            impact.Kind = EParticleKind::SpriteBurst;
            impact.BurstCount = bHitCharacter ? 12 : 8;
            impact.Lifetime = 0.12f;
            impact.Size = 0.04f;
            impact.SizeEnd = 0.01f;
            if (bHitCharacter) {
                impact.Color = {0.85f, 0.12f, 0.12f, 1.0f};
                impact.ColorEnd = {0.4f, 0.02f, 0.02f, 0.0f};
            } else {
                impact.Color = {0.85f, 0.8f, 0.55f, 1.0f};
                impact.ColorEnd = {0.45f, 0.4f, 0.3f, 0.0f};
            }
            impact.VelocityMin = {-0.8f, 0.2f, -0.8f};
            impact.VelocityMax = {0.8f, 1.6f, 0.8f};
            impact.Gravity = {0.0f, -8.0f, 0.0f};
            if (UGameplayStatics::SpawnEmitterAtLocation(Ctx.World, impact, InTraceEnd))
                ++spawnCount;
        }

        if (Ctx.OwnerCharacter && Ctx.OwnerCharacter->IsLocallyControlled())
            UGameplayStatics::PlaySound2D("/Game/Audio/SFX_RifleFire", 0.95f);
        else
            UGameplayStatics::PlaySoundAtLocation("/Game/Audio/SFX_RifleFire", InMuzzle, 0.9f, 4500.0f);
        if (bHitCharacter)
            UGameplayStatics::PlaySoundAtLocation("/Game/Audio/SFX_HitConfirm", InTraceEnd, 0.8f, 2800.0f);
        return spawnCount;
    }

    int FLeonTournamentWeaponVfx::SpawnRocketLaunchEffects(const FLeonTournamentWeaponVfxContext& Ctx, const glm::vec3& InMuzzle) {
        int spawnCount = 0;
        if (!Ctx.World)
            return spawnCount;

        FParticleEmitterSettings muzzle;
        muzzle.Kind = EParticleKind::SpriteBurst;
        muzzle.BurstCount = 28;
        muzzle.Lifetime = 0.16f;
        muzzle.Size = 0.1f;
        muzzle.SizeEnd = 0.32f;
        muzzle.Color = {1.0f, 0.55f, 0.15f, 1.0f};
        muzzle.ColorEnd = {0.8f, 0.15f, 0.05f, 0.0f};
        muzzle.VelocityMin = {-0.45f, -0.1f, -0.45f};
        muzzle.VelocityMax = {0.45f, 0.55f, 0.45f};
        if (UGameplayStatics::SpawnEmitterAtLocation(Ctx.World, muzzle, InMuzzle))
            ++spawnCount;

        FParticleEmitterSettings smoke;
        smoke.Kind = EParticleKind::SpriteBurst;
        smoke.BurstCount = 16;
        smoke.Lifetime = 0.35f;
        smoke.Size = 0.12f;
        smoke.SizeEnd = 0.4f;
        smoke.Color = {0.35f, 0.28f, 0.22f, 0.7f};
        smoke.ColorEnd = {0.08f, 0.06f, 0.05f, 0.0f};
        smoke.VelocityMin = {-0.3f, 0.1f, -0.3f};
        smoke.VelocityMax = {0.3f, 1.2f, 0.3f};
        smoke.Gravity = {0.0f, 1.5f, 0.0f};
        if (UGameplayStatics::SpawnEmitterAtLocation(Ctx.World, smoke, InMuzzle))
            ++spawnCount;

        if (Ctx.OwnerCharacter && Ctx.OwnerCharacter->IsLocallyControlled())
            UGameplayStatics::PlaySound2D("/Game/Audio/SFX_RifleFire", 1.0f);
        else
            UGameplayStatics::PlaySoundAtLocation("/Game/Audio/SFX_RifleFire", InMuzzle, 1.0f, 5000.0f);
        return spawnCount;
    }

    int FLeonTournamentWeaponVfx::SpawnLaserEffects(const FLeonTournamentWeaponVfxContext& Ctx, const glm::vec3& InMuzzle,
                          const glm::vec3& InTraceEnd, bool bHitCharacter) {
        int spawnCount = 0;
        if (!Ctx.World)
            return spawnCount;

        FParticleEmitterSettings muzzle;
        muzzle.Kind = EParticleKind::SpriteBurst;
        muzzle.BurstCount = 18;
        muzzle.Lifetime = 0.12f;
        muzzle.Size = 0.07f;
        muzzle.SizeEnd = 0.22f;
        muzzle.Color = {0.35f, 0.95f, 1.0f, 1.0f};
        muzzle.ColorEnd = {0.05f, 0.35f, 1.0f, 0.0f};
        muzzle.VelocityMin = {-0.2f, -0.1f, -0.2f};
        muzzle.VelocityMax = {0.2f, 0.3f, 0.2f};
        muzzle.Gravity = {0.0f, 0.0f, 0.0f};
        if (UGameplayStatics::SpawnEmitterAtLocation(Ctx.World, muzzle, InMuzzle))
            ++spawnCount;

        FParticleEmitterSettings beam;
        beam.Kind = EParticleKind::Beam;
        beam.Lifetime = 0.16f;
        beam.Color = {0.45f, 1.0f, 1.0f, 0.95f};
        beam.ColorEnd = {0.1f, 0.4f, 1.0f, 0.0f};
        beam.BeamEnd = InTraceEnd;
        beam.BeamThickness = 0.045f;
        if (UGameplayStatics::SpawnEmitterAtLocation(Ctx.World, beam, InMuzzle))
            ++spawnCount;

        FParticleEmitterSettings impact;
        impact.Kind = EParticleKind::SpriteBurst;
        impact.BurstCount = bHitCharacter ? 22 : 14;
        impact.Lifetime = 0.2f;
        impact.Size = 0.06f;
        impact.SizeEnd = 0.02f;
        impact.Color = bHitCharacter ? glm::vec4(1.0f, 0.2f, 0.35f, 1.0f) : glm::vec4(0.5f, 0.9f, 1.0f, 1.0f);
        impact.ColorEnd = {0.1f, 0.2f, 0.8f, 0.0f};
        impact.VelocityMin = {-1.4f, 0.2f, -1.4f};
        impact.VelocityMax = {1.4f, 2.2f, 1.4f};
        impact.Gravity = {0.0f, -6.0f, 0.0f};
        if (UGameplayStatics::SpawnEmitterAtLocation(Ctx.World, impact, InTraceEnd))
            ++spawnCount;

        if (Ctx.OwnerCharacter && Ctx.OwnerCharacter->IsLocallyControlled())
            UGameplayStatics::PlaySound2D("/Game/Audio/SFX_RifleFire", 1.15f);
        else
            UGameplayStatics::PlaySoundAtLocation("/Game/Audio/SFX_RifleFire", InMuzzle, 1.1f, 5200.0f);
        if (bHitCharacter)
            UGameplayStatics::PlaySoundAtLocation("/Game/Audio/SFX_HitConfirm", InTraceEnd, 0.9f, 3000.0f);
        return spawnCount;
    }

    int FLeonTournamentWeaponVfx::SpawnShotgunBlastEffects(const FLeonTournamentWeaponVfxContext& Ctx, const glm::vec3& InMuzzle,
                                 const glm::vec3& InAimDir, float InCurrentSpreadDeg,
                                 const std::function<glm::vec3(const glm::vec3&, float)>& ApplySpread) {
        int spawnCount = 0;
        if (!Ctx.World || !Ctx.Config)
            return spawnCount;

        const glm::vec3 n = glm::length(InAimDir) > 1e-5f ? glm::normalize(InAimDir) : glm::vec3(0.0f, 0.0f, 1.0f);

        FParticleEmitterSettings flash;
        flash.Kind = EParticleKind::SpriteBurst;
        flash.BurstCount = 28;
        flash.Lifetime = 0.1f;
        flash.Size = 0.08f;
        flash.SizeEnd = 0.22f;
        flash.Color = {1.0f, 0.85f, 0.35f, 1.0f};
        flash.ColorEnd = {1.0f, 0.35f, 0.05f, 0.0f};
        flash.VelocityMin = n * 2.0f + glm::vec3(-1.5f, -0.4f, -1.5f);
        flash.VelocityMax = n * 8.0f + glm::vec3(1.5f, 1.5f, 1.5f);
        flash.Gravity = {0.0f, -2.0f, 0.0f};
        if (UGameplayStatics::SpawnEmitterAtLocation(Ctx.World, flash, InMuzzle))
            ++spawnCount;

        FParticleEmitterSettings smoke;
        smoke.Kind = EParticleKind::SpriteBurst;
        smoke.BurstCount = 14;
        smoke.Lifetime = 0.35f;
        smoke.Size = 0.1f;
        smoke.SizeEnd = 0.35f;
        smoke.Color = {0.35f, 0.3f, 0.25f, 0.65f};
        smoke.ColorEnd = {0.08f, 0.07f, 0.06f, 0.0f};
        smoke.VelocityMin = n * 0.5f + glm::vec3(-0.5f, 0.2f, -0.5f);
        smoke.VelocityMax = n * 2.5f + glm::vec3(0.5f, 1.2f, 0.5f);
        smoke.Gravity = {0.0f, 1.5f, 0.0f};
        if (UGameplayStatics::SpawnEmitterAtLocation(Ctx.World, smoke, InMuzzle + n * 0.15f))
            ++spawnCount;

        if (ApplySpread) {
            for (int i = 0; i < 6; ++i) {
                const glm::vec3 dir = ApplySpread(n, InCurrentSpreadDeg + Ctx.Config->PelletSpreadDeg);
                FParticleEmitterSettings tracer;
                tracer.Kind = EParticleKind::Beam;
                tracer.Lifetime = 0.07f;
                tracer.Color = {1.0f, 0.9f, 0.45f, 0.7f};
                tracer.ColorEnd = {1.0f, 0.45f, 0.1f, 0.0f};
                tracer.BeamEnd = InMuzzle + dir * 4.5f;
                tracer.BeamThickness = 0.014f;
                UGameplayStatics::SpawnEmitterAtLocation(Ctx.World, tracer, InMuzzle + dir * 0.1f);
            }
        }

        if (Ctx.OwnerCharacter && Ctx.OwnerCharacter->IsLocallyControlled())
            UGameplayStatics::PlaySound2D("/Game/Audio/SFX_RifleFire", 1.05f);
        else
            UGameplayStatics::PlaySoundAtLocation("/Game/Audio/SFX_RifleFire", InMuzzle, 1.0f, 4800.0f);
        return spawnCount;
    }

    int FLeonTournamentWeaponVfx::SpawnFlameEffects(const FLeonTournamentWeaponVfxContext& Ctx, const glm::vec3& InMuzzle,
                          const glm::vec3& InDir) {
        int spawnCount = 0;
        if (!Ctx.World || !Ctx.Config)
            return spawnCount;

        const glm::vec3 n = glm::length(InDir) > 1e-5f ? glm::normalize(InDir) : glm::vec3(0.0f, 0.0f, 1.0f);
        // Lifetime * forward speed ≈ Config.Range (2 m) so the spray visually matches damage reach.
        const float reach = std::max(0.5f, Ctx.Config->Range);
        const float life = 0.42f;
        const float speedMin = reach / life * 0.85f;
        const float speedMax = reach / life * 1.15f;

        FParticleEmitterSettings flame;
        flame.Kind = EParticleKind::SpriteBurst;
        flame.BurstCount = 48;
        flame.Lifetime = life;
        flame.Size = 0.18f;
        flame.SizeEnd = 0.55f;
        flame.Color = {1.0f, 0.55f, 0.08f, 1.0f};
        flame.ColorEnd = {0.35f, 0.05f, 0.0f, 0.0f};
        flame.VelocityMin = n * speedMin + glm::vec3(-1.8f, -0.35f, -1.8f);
        flame.VelocityMax = n * speedMax + glm::vec3(1.8f, 2.0f, 1.8f);
        flame.Gravity = {0.0f, 1.8f, 0.0f};
        if (UGameplayStatics::SpawnEmitterAtLocation(Ctx.World, flame, InMuzzle + n * 0.2f))
            ++spawnCount;

        FParticleEmitterSettings core;
        core.Kind = EParticleKind::SpriteBurst;
        core.BurstCount = 22;
        core.Lifetime = life * 0.85f;
        core.Size = 0.12f;
        core.SizeEnd = 0.38f;
        core.Color = {1.0f, 0.85f, 0.25f, 1.0f};
        core.ColorEnd = {1.0f, 0.25f, 0.02f, 0.0f};
        core.VelocityMin = n * (speedMin * 0.9f) + glm::vec3(-0.9f, -0.15f, -0.9f);
        core.VelocityMax = n * (speedMax * 1.05f) + glm::vec3(0.9f, 1.1f, 0.9f);
        core.Gravity = {0.0f, 1.2f, 0.0f};
        if (UGameplayStatics::SpawnEmitterAtLocation(Ctx.World, core, InMuzzle + n * 0.15f))
            ++spawnCount;

        FParticleEmitterSettings smoke;
        smoke.Kind = EParticleKind::SpriteBurst;
        smoke.BurstCount = 18;
        smoke.Lifetime = 0.55f;
        smoke.Size = 0.22f;
        smoke.SizeEnd = 0.65f;
        smoke.Color = {0.28f, 0.18f, 0.1f, 0.6f};
        smoke.ColorEnd = {0.05f, 0.05f, 0.05f, 0.0f};
        smoke.VelocityMin = n * (speedMin * 0.55f) + glm::vec3(-1.0f, 0.5f, -1.0f);
        smoke.VelocityMax = n * (speedMax * 0.75f) + glm::vec3(1.0f, 2.2f, 1.0f);
        smoke.Gravity = {0.0f, 2.4f, 0.0f};
        UGameplayStatics::SpawnEmitterAtLocation(Ctx.World, smoke, InMuzzle + n * 0.35f);

        if (Ctx.OwnerCharacter && Ctx.OwnerCharacter->IsLocallyControlled())
            UGameplayStatics::PlaySound2D("/Game/Audio/SFX_RifleFire", 0.35f);
        return spawnCount;
    }

} // namespace Leon
