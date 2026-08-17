#pragma once

#include <cstdint>
#include <string>
#include <glm/glm.hpp>

namespace Leon {

    enum class ELeonTournamentTeam : uint8_t { None = 0, Team1 = 1, Team2 = 2 };

    enum class ELeonTournamentMatchState : uint8_t { MainMenu = 0, Lobby = 1, Starting = 2, Playing = 3, Finished = 4 };

    enum class ELeonTournamentMatchWinner : uint8_t { None = 0, Team1 = 1, Team2 = 2, Draw = 3 };

    enum class ELeonTournamentSessionMode : uint8_t { Offline = 0, LanHost = 1, LanClient = 2 };

    /** Selectable pawn mesh; each skin keeps its own .lskeleton. Mixamo anims link by bone name. */
    enum class ELeonTournamentCharacterSkin : uint8_t {
        YBot = 0,
        Patrick = 1,
        Trump = 2,
        Count = 3
    };

    inline const char* LeonTournamentCharacterSkinName(ELeonTournamentCharacterSkin InSkin) {
        switch (InSkin) {
        case ELeonTournamentCharacterSkin::Patrick:
            return "PATRICK";
        case ELeonTournamentCharacterSkin::Trump:
            return "TRUMP";
        case ELeonTournamentCharacterSkin::YBot:
        default:
            return "YBOT";
        }
    }

    inline const char* LeonTournamentCharacterMeshPath(ELeonTournamentCharacterSkin InSkin) {
        switch (InSkin) {
        case ELeonTournamentCharacterSkin::Patrick:
            return "/Game/SkeletalMeshes/Patrick.lskeletalmesh";
        case ELeonTournamentCharacterSkin::Trump:
            return "/Game/SkeletalMeshes/Trump.lskeletalmesh";
        case ELeonTournamentCharacterSkin::YBot:
        default:
            return "/Game/SkeletalMeshes/YBot.lskeletalmesh";
        }
    }

    inline ELeonTournamentCharacterSkin LeonTournamentClampCharacterSkin(ELeonTournamentCharacterSkin InSkin) {
        const uint8_t count = static_cast<uint8_t>(ELeonTournamentCharacterSkin::Count);
        uint8_t v = static_cast<uint8_t>(InSkin);
        if (v >= count)
            v = 0;
        return static_cast<ELeonTournamentCharacterSkin>(v);
    }

    /** Standing height in meters for every selectable skin. */
    inline float LeonTournamentCharacterTargetHeightMeters(ELeonTournamentCharacterSkin /*InSkin*/) {
        return 1.80f;
    }

    inline ELeonTournamentCharacterSkin LeonTournamentNextCharacterSkin(ELeonTournamentCharacterSkin InSkin) {
        const uint8_t next = static_cast<uint8_t>(
            (static_cast<uint8_t>(InSkin) + 1) % static_cast<uint8_t>(ELeonTournamentCharacterSkin::Count));
        return static_cast<ELeonTournamentCharacterSkin>(next);
    }

    inline ELeonTournamentCharacterSkin LeonTournamentPrevCharacterSkin(ELeonTournamentCharacterSkin InSkin) {
        const uint8_t count = static_cast<uint8_t>(ELeonTournamentCharacterSkin::Count);
        const uint8_t cur = static_cast<uint8_t>(InSkin);
        return static_cast<ELeonTournamentCharacterSkin>((cur + count - 1) % count);
    }

    enum class ELeonTournamentBotState : uint8_t {
        Idle = 0,
        Search = 1,
        MoveToTarget = 2,
        Combat = 3,
        TakeCover = 4,
        Aim = 5,
        Fire = 6,
        Reload = 7,
        Dead = 8,
        Respawn = 9
    };

    inline const char* LeonTournamentBotStateName(ELeonTournamentBotState InState) {
        switch (InState) {
        case ELeonTournamentBotState::Idle:
            return "IDLE";
        case ELeonTournamentBotState::Search:
            return "SEARCH";
        case ELeonTournamentBotState::MoveToTarget:
            return "MOVE";
        case ELeonTournamentBotState::Combat:
            return "COMBAT";
        case ELeonTournamentBotState::TakeCover:
            return "COVER";
        case ELeonTournamentBotState::Aim:
            return "AIM";
        case ELeonTournamentBotState::Fire:
            return "FIRE";
        case ELeonTournamentBotState::Reload:
            return "RELOAD";
        case ELeonTournamentBotState::Dead:
            return "DEAD";
        case ELeonTournamentBotState::Respawn:
            return "RESPAWN";
        default:
            return "?";
        }
    }

    inline const char* LeonTournamentTeamName(ELeonTournamentTeam InTeam) {
        switch (InTeam) {
        case ELeonTournamentTeam::Team1:
            return "TEAM 1";
        case ELeonTournamentTeam::Team2:
            return "TEAM 2";
        default:
            return "NONE";
        }
    }

    struct FLeonTournamentMatchConfig {
        float MatchDurationSeconds = 600.0f;
        int32_t ScoreLimit = 25;
        int32_t MaxTeamSize = 6;
        int32_t MaxPlayers = 12;
        float AssistWindowSeconds = 5.0f;
        float RespawnDelaySeconds = 1.5f;
        float StartCountdownSeconds = 2.0f;
        bool bFriendlyFire = false;
    };

    enum class ELeonTournamentWeaponId : uint8_t {
        Rifle = 0,
        Shotgun = 1,
        Rocket = 2,
        Laser = 3,
        Grenade = 4,
        Flamethrower = 5,
        Count = 6
    };

    enum class ELeonTournamentFireMode : uint8_t { Hitscan = 0, Projectile = 1, Flame = 2 };

    enum class ELeonTournamentCrosshairStyle : uint8_t { Cross = 0, Circle = 1 };

    inline const char* LeonTournamentWeaponName(ELeonTournamentWeaponId InId) {
        switch (InId) {
        case ELeonTournamentWeaponId::Shotgun:
            return "SHOTGUN";
        case ELeonTournamentWeaponId::Rocket:
            return "ROCKET";
        case ELeonTournamentWeaponId::Laser:
            return "LASER";
        case ELeonTournamentWeaponId::Grenade:
            return "GRENADE";
        case ELeonTournamentWeaponId::Flamethrower:
            return "FLAMER";
        case ELeonTournamentWeaponId::Rifle:
        default:
            return "RIFLE";
        }
    }

    struct FLeonTournamentWeaponConfig {
        float FireRate = 12.0f;
        float Damage = 18.0f;
        int32_t MagazineSize = 40;
        float ReloadTime = 1.45f;
        float Range = 200.0f;
        /** Half-angle cone (degrees) at rest; bloom opens the random aim field. */
        float BaseSpreadDeg = 0.35f;
        float MaxSpreadDeg = 2.8f;
        float SpreadPerShotDeg = 0.45f;
        float SpreadRecoveryPerSec = 8.0f;
        float RecoilPitchDeg = 0.45f;
        int32_t PelletCount = 1;
        float PelletSpreadDeg = 0.0f;
        float ProjectileSpeed = 28.0f;
        float ProjectileRadius = 0.18f;
        float SplashRadius = 3.2f;
        float SplashDamage = 40.0f;
        ELeonTournamentFireMode FireMode = ELeonTournamentFireMode::Hitscan;
        float ProjectileGravityScale = 0.0f;
        float Knockback = 8.0f;
        float ScopeFOV = 0.0f;
        float FlameConeDeg = 8.0f;
        /** Extra world-surface reflections after the first hitscan impact (characters stop the ray). */
        int32_t RicochetBounces = 0;
        ELeonTournamentCrosshairStyle CrosshairStyle = ELeonTournamentCrosshairStyle::Cross;
        glm::vec3 VisualColor{0.12f, 0.12f, 0.14f};
        float VisualRadius = 0.035f;
        float VisualLength = 0.42f;
    };

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

    /** @deprecated Prefer FLeonTournamentWeaponConfig. */
    using FLeonTournamentRifleConfig = FLeonTournamentWeaponConfig;

    /** Grip / muzzle socket for ~1.80 m Mixamo characters (meters, Y-up). */
    inline constexpr float LeonTournamentFireHeightFromGround = 0.90f;
    inline constexpr float LeonTournamentFireRightOffset = 0.22f;
    inline constexpr float LeonTournamentFireForwardOffset = 0.20f;

    struct FLeonTournamentBotPersonality {
        float Aggression = 0.55f;
        float Accuracy = 0.6f;
        float ReactionTime = 0.22f;
        float PreferredRange = 15.0f;
        float StrafeFrequency = 1.1f;
        float CoverPreference = 0.45f;
        int32_t Tactic = 0;
    };

} // namespace Leon
