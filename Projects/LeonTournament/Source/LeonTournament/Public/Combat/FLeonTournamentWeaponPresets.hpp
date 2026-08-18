#pragma once

#include "FLeonTournamentTypes.hpp"

namespace Leon {

    /**
     * Arena presets vs 100 HP (no armor).
     * Rifle is the always-on mid TTK. Shotgun can drop at point-blank if most pellets land.
     * Rocket / grenade / laser take two connecting hits from full health. Flame melts only in close cone.
     */
    inline FLeonTournamentWeaponConfig LeonTournamentWeaponPreset(ELeonTournamentWeaponId InId) {
        FLeonTournamentWeaponConfig c;
        switch (InId) {
        case ELeonTournamentWeaponId::Shotgun:
            c.FireRate = 1.15f;
            c.Damage = 14.0f;
            c.MagazineSize = 6;
            c.ReloadTime = 2.0f;
            c.Range = 22.0f;
            c.BaseSpreadDeg = 5.5f;
            c.MaxSpreadDeg = 8.0f;
            c.SpreadPerShotDeg = 1.4f;
            c.SpreadRecoveryPerSec = 4.5f;
            c.RecoilPitchDeg = 2.2f;
            c.PelletCount = 9;
            c.PelletSpreadDeg = 6.8f;
            c.FireMode = ELeonTournamentFireMode::Projectile;
            c.ProjectileSpeed = 58.0f;
            c.ProjectileRadius = 0.05f;
            c.ProjectileGravityScale = 0.08f;
            c.SplashRadius = 0.0f;
            c.SplashDamage = 0.0f;
            c.RicochetBounces = 1;
            c.Knockback = 5.5f;
            c.CrosshairStyle = ELeonTournamentCrosshairStyle::Circle;
            c.VisualColor = {1.0f, 0.82f, 0.35f};
            c.VisualRadius = 0.045f;
            c.VisualLength = 0.36f;
            break;
        case ELeonTournamentWeaponId::Rocket:
            c.FireRate = 0.80f;
            c.Damage = 78.0f;
            c.MagazineSize = 5;
            c.ReloadTime = 2.15f;
            c.Range = 120.0f;
            c.BaseSpreadDeg = 0.2f;
            c.MaxSpreadDeg = 0.7f;
            c.SpreadPerShotDeg = 0.22f;
            c.SpreadRecoveryPerSec = 4.0f;
            c.RecoilPitchDeg = 1.35f;
            c.PelletCount = 1;
            c.FireMode = ELeonTournamentFireMode::Projectile;
            c.ProjectileSpeed = 30.0f;
            c.ProjectileRadius = 0.22f;
            c.ProjectileGravityScale = 0.0f;
            c.SplashRadius = 4.8f;
            c.SplashDamage = 48.0f;
            c.Knockback = 20.0f;
            c.CrosshairStyle = ELeonTournamentCrosshairStyle::Cross;
            c.VisualColor = {0.55f, 0.12f, 0.12f};
            c.VisualRadius = 0.05f;
            c.VisualLength = 0.48f;
            break;
        case ELeonTournamentWeaponId::Laser:
            c.FireRate = 0.70f;
            c.Damage = 62.0f;
            c.MagazineSize = 3;
            c.ReloadTime = 1.40f;
            c.Range = 220.0f;
            c.BaseSpreadDeg = 0.0f;
            c.MaxSpreadDeg = 0.12f;
            c.SpreadPerShotDeg = 0.0f;
            c.SpreadRecoveryPerSec = 12.0f;
            c.RecoilPitchDeg = 0.55f;
            c.PelletCount = 1;
            c.Knockback = 5.0f;
            c.ScopeFOV = 32.0f;
            c.CrosshairStyle = ELeonTournamentCrosshairStyle::Cross;
            c.VisualColor = {0.15f, 0.85f, 1.0f};
            c.VisualRadius = 0.028f;
            c.VisualLength = 0.52f;
            break;
        case ELeonTournamentWeaponId::Grenade:
            c.FireRate = 0.95f;
            c.Damage = 52.0f;
            c.MagazineSize = 5;
            c.ReloadTime = 2.2f;
            c.Range = 70.0f;
            c.BaseSpreadDeg = 0.5f;
            c.MaxSpreadDeg = 1.6f;
            c.SpreadPerShotDeg = 0.28f;
            c.SpreadRecoveryPerSec = 5.0f;
            c.RecoilPitchDeg = 1.5f;
            c.PelletCount = 1;
            c.FireMode = ELeonTournamentFireMode::Projectile;
            c.ProjectileSpeed = 17.0f;
            c.ProjectileRadius = 0.16f;
            c.ProjectileGravityScale = 0.70f;
            c.SplashRadius = 4.0f;
            c.SplashDamage = 38.0f;
            c.Knockback = 18.0f;
            c.CrosshairStyle = ELeonTournamentCrosshairStyle::Circle;
            c.VisualColor = {0.22f, 0.55f, 0.18f};
            c.VisualRadius = 0.055f;
            c.VisualLength = 0.34f;
            break;
        case ELeonTournamentWeaponId::Flamethrower:
            c.FireRate = 12.0f;
            c.Damage = 9.0f;
            c.MagazineSize = 60;
            c.ReloadTime = 2.3f;
            c.Range = 9.0f;
            c.BaseSpreadDeg = 1.2f;
            c.MaxSpreadDeg = 3.0f;
            c.SpreadPerShotDeg = 0.15f;
            c.SpreadRecoveryPerSec = 10.0f;
            c.RecoilPitchDeg = 0.04f;
            c.PelletCount = 1;
            c.PelletSpreadDeg = 0.0f;
            c.FireMode = ELeonTournamentFireMode::Flame;
            c.FlameConeDeg = 8.0f;
            c.Knockback = 1.8f;
            c.CrosshairStyle = ELeonTournamentCrosshairStyle::Circle;
            c.VisualColor = {1.0f, 0.35f, 0.05f};
            c.VisualRadius = 0.05f;
            c.VisualLength = 0.4f;
            break;
        case ELeonTournamentWeaponId::Rifle:
        default:
            c.FireRate = 7.5f;
            c.Damage = 16.0f;
            c.MagazineSize = 24;
            c.ReloadTime = 1.40f;
            c.Range = 180.0f;
            c.BaseSpreadDeg = 0.42f;
            c.MaxSpreadDeg = 3.0f;
            c.SpreadPerShotDeg = 0.48f;
            c.SpreadRecoveryPerSec = 7.5f;
            c.RecoilPitchDeg = 0.38f;
            c.PelletCount = 1;
            c.CrosshairStyle = ELeonTournamentCrosshairStyle::Cross;
            c.VisualColor = {0.12f, 0.12f, 0.14f};
            break;
        }
        return c;
    }

} // namespace Leon
