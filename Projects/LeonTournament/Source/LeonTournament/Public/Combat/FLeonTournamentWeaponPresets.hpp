#pragma once

#include "FLeonTournamentTypes.hpp"

namespace Leon {

    /** Frenetic UT-style presets. */
    inline FLeonTournamentWeaponConfig LeonTournamentWeaponPreset(ELeonTournamentWeaponId InId) {
        FLeonTournamentWeaponConfig c;
        switch (InId) {
        case ELeonTournamentWeaponId::Shotgun:
            c.FireRate = 1.35f;
            c.Damage = 12.0f;
            c.MagazineSize = 8;
            c.ReloadTime = 1.8f;
            c.Range = 45.0f;
            c.BaseSpreadDeg = 4.5f;
            c.MaxSpreadDeg = 7.0f;
            c.SpreadPerShotDeg = 1.2f;
            c.SpreadRecoveryPerSec = 5.0f;
            c.RecoilPitchDeg = 1.8f;
            c.PelletCount = 8;
            c.PelletSpreadDeg = 5.5f;
            c.FireMode = ELeonTournamentFireMode::Projectile;
            c.ProjectileSpeed = 95.0f;
            c.ProjectileRadius = 0.05f;
            c.ProjectileGravityScale = 0.06f;
            c.SplashRadius = 0.0f;
            c.SplashDamage = 0.0f;
            c.RicochetBounces = 1;
            c.Knockback = 4.0f;
            c.CrosshairStyle = ELeonTournamentCrosshairStyle::Circle;
            c.VisualColor = {1.0f, 0.82f, 0.35f};
            c.VisualRadius = 0.045f;
            c.VisualLength = 0.36f;
            break;
        case ELeonTournamentWeaponId::Rocket:
            c.FireRate = 0.85f;
            c.Damage = 100.0f;
            c.MagazineSize = 6;
            c.ReloadTime = 2.0f;
            c.Range = 120.0f;
            c.BaseSpreadDeg = 0.15f;
            c.MaxSpreadDeg = 0.6f;
            c.SpreadPerShotDeg = 0.2f;
            c.SpreadRecoveryPerSec = 4.0f;
            c.RecoilPitchDeg = 1.2f;
            c.PelletCount = 1;
            c.FireMode = ELeonTournamentFireMode::Projectile;
            c.ProjectileSpeed = 28.0f;
            c.ProjectileRadius = 0.22f;
            c.ProjectileGravityScale = 0.0f;
            c.SplashRadius = 5.8f;
            c.SplashDamage = 62.0f;
            c.Knockback = 26.0f;
            c.CrosshairStyle = ELeonTournamentCrosshairStyle::Cross;
            c.VisualColor = {0.55f, 0.12f, 0.12f};
            c.VisualRadius = 0.05f;
            c.VisualLength = 0.48f;
            break;
        case ELeonTournamentWeaponId::Laser:
            c.FireRate = 0.55f;
            c.Damage = 125.0f;
            c.MagazineSize = 1;
            c.ReloadTime = 1.35f;
            c.Range = 220.0f;
            c.BaseSpreadDeg = 0.0f;
            c.MaxSpreadDeg = 0.15f;
            c.SpreadPerShotDeg = 0.0f;
            c.SpreadRecoveryPerSec = 12.0f;
            c.RecoilPitchDeg = 0.8f;
            c.PelletCount = 1;
            c.Knockback = 6.0f;
            c.ScopeFOV = 32.0f;
            c.CrosshairStyle = ELeonTournamentCrosshairStyle::Cross;
            c.VisualColor = {0.15f, 0.85f, 1.0f};
            c.VisualRadius = 0.028f;
            c.VisualLength = 0.52f;
            break;
        case ELeonTournamentWeaponId::Grenade:
            c.FireRate = 1.1f;
            c.Damage = 85.0f;
            c.MagazineSize = 6;
            c.ReloadTime = 2.1f;
            c.Range = 80.0f;
            c.BaseSpreadDeg = 0.4f;
            c.MaxSpreadDeg = 1.4f;
            c.SpreadPerShotDeg = 0.25f;
            c.SpreadRecoveryPerSec = 5.0f;
            c.RecoilPitchDeg = 1.4f;
            c.PelletCount = 1;
            c.FireMode = ELeonTournamentFireMode::Projectile;
            c.ProjectileSpeed = 16.0f;
            c.ProjectileRadius = 0.16f;
            c.ProjectileGravityScale = 0.55f;
            c.SplashRadius = 4.4f;
            c.SplashDamage = 55.0f;
            c.Knockback = 26.0f;
            c.CrosshairStyle = ELeonTournamentCrosshairStyle::Circle;
            c.VisualColor = {0.22f, 0.55f, 0.18f};
            c.VisualRadius = 0.055f;
            c.VisualLength = 0.34f;
            break;
        case ELeonTournamentWeaponId::Flamethrower:
            c.FireRate = 14.0f;
            c.Damage = 7.0f;
            c.MagazineSize = 80;
            c.ReloadTime = 2.4f;
            c.Range = 2.0f;
            c.BaseSpreadDeg = 4.0f;
            c.MaxSpreadDeg = 8.0f;
            c.SpreadPerShotDeg = 0.4f;
            c.SpreadRecoveryPerSec = 10.0f;
            c.RecoilPitchDeg = 0.05f;
            c.PelletCount = 6;
            c.PelletSpreadDeg = 10.0f;
            c.FireMode = ELeonTournamentFireMode::Flame;
            c.FlameConeDeg = 14.0f;
            c.Knockback = 1.5f;
            c.CrosshairStyle = ELeonTournamentCrosshairStyle::Circle;
            c.VisualColor = {1.0f, 0.35f, 0.05f};
            c.VisualRadius = 0.05f;
            c.VisualLength = 0.4f;
            break;
        case ELeonTournamentWeaponId::Rifle:
        default:
            c.FireRate = 6.0f;
            c.Damage = 18.0f;
            c.MagazineSize = 20;
            c.ReloadTime = 1.45f;
            c.Range = 200.0f;
            c.BaseSpreadDeg = 0.35f;
            c.MaxSpreadDeg = 2.6f;
            c.SpreadPerShotDeg = 0.4f;
            c.SpreadRecoveryPerSec = 8.0f;
            c.RecoilPitchDeg = 0.4f;
            c.PelletCount = 1;
            c.CrosshairStyle = ELeonTournamentCrosshairStyle::Cross;
            c.VisualColor = {0.12f, 0.12f, 0.14f};
            break;
        }
        return c;
    }

} // namespace Leon
