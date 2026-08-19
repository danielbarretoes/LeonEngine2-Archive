#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <glm/glm.hpp>

namespace Leon {

    enum class ELeonTournamentTeam : uint8_t { None = 0, Team1 = 1, Team2 = 2 };

    enum class ELeonTournamentMatchState : uint8_t { MainMenu = 0, Lobby = 1, Starting = 2, Playing = 3, Finished = 4 };

    enum class ELeonTournamentMatchWinner : uint8_t { None = 0, Team1 = 1, Team2 = 2, Draw = 3 };

    enum class ELeonTournamentSessionMode : uint8_t { Offline = 0, LanHost = 1, LanClient = 2 };

    enum class ELeonTournamentPlayableMap : uint8_t { Arena = 0, NightArena = 1, Count = 2 };

    inline const char* LeonTournamentPlayableMapName(ELeonTournamentPlayableMap InMap) {
        switch (InMap) {
        case ELeonTournamentPlayableMap::NightArena:
            return "NIGHT ARENA";
        case ELeonTournamentPlayableMap::Arena:
        default:
            return "TOURNAMENT ARENA";
        }
    }

    inline const char* LeonTournamentPlayableMapPath(ELeonTournamentPlayableMap InMap) {
        switch (InMap) {
        case ELeonTournamentPlayableMap::NightArena:
            return "/Game/Maps/TournamentArenaNight";
        case ELeonTournamentPlayableMap::Arena:
        default:
            return "/Game/Maps/TournamentArena";
        }
    }

    inline ELeonTournamentPlayableMap LeonTournamentClampPlayableMap(ELeonTournamentPlayableMap InMap) {
        const uint8_t count = static_cast<uint8_t>(ELeonTournamentPlayableMap::Count);
        uint8_t v = static_cast<uint8_t>(InMap);
        if (v >= count)
            v = 0;
        return static_cast<ELeonTournamentPlayableMap>(v);
    }

    inline ELeonTournamentPlayableMap LeonTournamentNextPlayableMap(ELeonTournamentPlayableMap InMap) {
        const uint8_t next = static_cast<uint8_t>((static_cast<uint8_t>(InMap) + 1) %
                                                  static_cast<uint8_t>(ELeonTournamentPlayableMap::Count));
        return static_cast<ELeonTournamentPlayableMap>(next);
    }

    inline ELeonTournamentPlayableMap LeonTournamentPrevPlayableMap(ELeonTournamentPlayableMap InMap) {
        const uint8_t count = static_cast<uint8_t>(ELeonTournamentPlayableMap::Count);
        const uint8_t cur = static_cast<uint8_t>(InMap);
        return static_cast<ELeonTournamentPlayableMap>((cur + count - 1) % count);
    }

    enum class ELeonTournamentGameModeId : uint8_t { TeamDeathmatch = 0, FreeForAll = 1, CaptureTheFlag = 2, Count = 3 };

    inline const char* LeonTournamentGameModeName(ELeonTournamentGameModeId InMode) {
        switch (InMode) {
        case ELeonTournamentGameModeId::FreeForAll:
            return "FFA";
        case ELeonTournamentGameModeId::CaptureTheFlag:
            return "CTF";
        case ELeonTournamentGameModeId::TeamDeathmatch:
        default:
            return "TDM";
        }
    }

    enum class ELeonTournamentFlagStatus : uint8_t { AtBase = 0, Carried = 1, Dropped = 2 };

    inline const char* LeonTournamentFlagStatusName(ELeonTournamentFlagStatus InStatus) {
        switch (InStatus) {
        case ELeonTournamentFlagStatus::Carried:
            return "CARRIED";
        case ELeonTournamentFlagStatus::Dropped:
            return "DROPPED";
        case ELeonTournamentFlagStatus::AtBase:
        default:
            return "AT BASE";
        }
    }

    struct FLeonTournamentFlagState {
        ELeonTournamentTeam OwnerTeam = ELeonTournamentTeam::None;
        ELeonTournamentFlagStatus Status = ELeonTournamentFlagStatus::AtBase;
    };

    inline ELeonTournamentGameModeId LeonTournamentClampGameModeId(ELeonTournamentGameModeId InMode) {
        const uint8_t count = static_cast<uint8_t>(ELeonTournamentGameModeId::Count);
        uint8_t v = static_cast<uint8_t>(InMode);
        if (v >= count)
            v = 0;
        return static_cast<ELeonTournamentGameModeId>(v);
    }

    inline ELeonTournamentGameModeId LeonTournamentNextGameModeId(ELeonTournamentGameModeId InMode) {
        const uint8_t next = static_cast<uint8_t>((static_cast<uint8_t>(InMode) + 1) %
                                                  static_cast<uint8_t>(ELeonTournamentGameModeId::Count));
        return static_cast<ELeonTournamentGameModeId>(next);
    }

    inline ELeonTournamentGameModeId LeonTournamentPrevGameModeId(ELeonTournamentGameModeId InMode) {
        const uint8_t count = static_cast<uint8_t>(ELeonTournamentGameModeId::Count);
        const uint8_t cur = static_cast<uint8_t>(InMode);
        return static_cast<ELeonTournamentGameModeId>((cur + count - 1) % count);
    }

    inline constexpr char kLeonTournamentMenuShowcaseActorName[] = "MenuShowcase";
    inline constexpr char kLeonTournamentMenuShowcaseFloorName[] = "MenuShowcaseFloor";
    inline constexpr float kLeonTournamentMenuPanelDesignWidth = 420.0f;
    inline constexpr float kLeonTournamentLobbyPanelDesignWidth = 520.0f;
    /** Fallback view is 3.5 m / -10° (above a standing pawn). Menu looks at mid-torso instead. */
    inline constexpr glm::vec3 kLeonTournamentMenuCameraPosition{0.0f, 1.4f, 10.5f};
    inline constexpr float kLeonTournamentMenuCameraPitchDeg = -8.0f;
    inline constexpr float kLeonTournamentMenuCameraYawDeg = -90.0f;
    inline constexpr float kLeonTournamentMenuStandDistance = 3.5f;

    inline float LeonTournamentMenuPanelWidthPx(float InViewportW, bool bLobby) {
        const float design = bLobby ? kLeonTournamentLobbyPanelDesignWidth : kLeonTournamentMenuPanelDesignWidth;
        return std::min(design, InViewportW * 0.62f);
    }

    /**
     * Stand location for the menu/lobby preview pawn: on the floor, in front of the
     * fallback camera, and inside the frustum to the right of the left UI panel.
     */
    inline glm::vec3 LeonTournamentMenuShowcaseLocation(const glm::vec3& InCamPos, const glm::vec3& InCamForward,
                                                        const glm::vec3& InCamRight, float InFovDegrees, float InAspect,
                                                        float InCapsuleHalfHeight, float InViewportWidth,
                                                        float InPanelWidthPx) {
        const float aspect = std::max(InAspect, 1e-4f);
        const float tanHalfV = std::tan(InFovDegrees * 0.00872664626f);
        const float tanHalfH = tanHalfV * aspect;

        glm::vec3 planarFwd(InCamForward.x, 0.0f, InCamForward.z);
        const float fwdLen = glm::length(planarFwd);
        planarFwd = fwdLen > 1e-4f ? planarFwd / fwdLen : glm::vec3(0.0f, 0.0f, -1.0f);
        glm::vec3 planarRight(InCamRight.x, 0.0f, InCamRight.z);
        const float rightLen = glm::length(planarRight);
        planarRight = rightLen > 1e-4f ? planarRight / rightLen : glm::vec3(1.0f, 0.0f, 0.0f);

        glm::vec3 stand(InCamPos.x, InCapsuleHalfHeight, InCamPos.z);
        stand += planarFwd * kLeonTournamentMenuStandDistance;

        const glm::vec3 toStand = stand - InCamPos;
        const float viewZ = std::max(glm::dot(toStand, InCamForward), 1.25f);
        const float halfW = tanHalfH * viewZ;
        const float panelFrac = std::clamp(InPanelWidthPx / std::max(InViewportWidth, 1.0f), 0.0f, 0.62f);
        const float u = glm::mix(panelFrac, 1.0f, 0.55f);
        const float ndcX = std::clamp(u * 2.0f - 1.0f, -0.72f, 0.72f);
        stand += planarRight * (ndcX * halfW);
        stand.y = InCapsuleHalfHeight;
        return stand;
    }

    /** Selectable pawn mesh; each skin keeps its own .lskeleton. Mixamo anims link by bone name. */
    enum class ELeonTournamentCharacterSkin : uint8_t { YBot = 0, Patrick = 1, Trump = 2, Count = 3 };

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
        const uint8_t next = static_cast<uint8_t>((static_cast<uint8_t>(InSkin) + 1) %
                                                  static_cast<uint8_t>(ELeonTournamentCharacterSkin::Count));
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
        float MatchDurationSeconds = 420.0f;
        int32_t ScoreLimit = 15;
        int32_t MaxTeamSize = 6;
        int32_t MaxPlayers = 12;
        float AssistWindowSeconds = 5.0f;
        float RespawnDelaySeconds = 1.0f;
        /** UT-style warmup: combat allowed, scoring blocked until Playing. */
        float StartCountdownSeconds = 5.0f;
        float SpawnProtectionSeconds = 2.5f;
        bool bFriendlyFire = false;
    };

    enum class ELeonTournamentBotDifficulty : uint8_t { Casual = 0, Normal = 1, Hard = 2 };

    inline const char* LeonTournamentBotDifficultyName(ELeonTournamentBotDifficulty InDifficulty) {
        switch (InDifficulty) {
        case ELeonTournamentBotDifficulty::Casual:
            return "CASUAL";
        case ELeonTournamentBotDifficulty::Hard:
            return "HARD";
        case ELeonTournamentBotDifficulty::Normal:
        default:
            return "NORMAL";
        }
    }

    enum class ELeonTournamentKillFeedKind : uint8_t { Kill = 0, Assist = 1, Streak = 2 };

    struct FLeonTournamentKillFeedEntry {
        std::string InstigatorName;
        std::string VictimName;
        ELeonTournamentKillFeedKind Kind = ELeonTournamentKillFeedKind::Kill;
        float TimeRemaining = 4.0f;
    };

    enum class ELeonTournamentWeaponId : uint8_t {
        Rifle = 0,
        Shotgun = 1,
        Rocket = 2,
        Laser = 3,
        Flamethrower = 4,
        Count = 5
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
        case ELeonTournamentWeaponId::Flamethrower:
            return "FLAMER";
        case ELeonTournamentWeaponId::Rifle:
        default:
            return "RIFLE";
        }
    }

    struct FLeonTournamentWeaponConfig {
        float FireRate = 7.5f;
        float Damage = 16.0f;
        int32_t MagazineSize = 24;
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
        /** Optional virtual asset path (/Game/...). Empty = built-in procedural crosshair per weapon. */
        std::string CrosshairTexturePath;
        glm::vec3 VisualColor{0.12f, 0.12f, 0.14f};
        float VisualRadius = 0.035f;
        float VisualLength = 0.42f;
    };

    /** @deprecated Prefer FLeonTournamentWeaponConfig. */
    using FLeonTournamentRifleConfig = FLeonTournamentWeaponConfig;

    /** Grip / muzzle socket for ~1.80 m Mixamo characters (meters, Y-up). */
    inline constexpr float LeonTournamentFireHeightFromGround = 0.90f;
    inline constexpr float LeonTournamentFireRightOffset = 0.22f;
    inline constexpr float LeonTournamentFireForwardOffset = 0.20f;
    /** Hitscan / pellet hits at or above the eyes count as headshots. */
    inline constexpr float kLeonTournamentHeadshotMultiplier = 2.0f;
    inline constexpr float kLeonTournamentHeadshotChinBelowEyeMeters = 0.10f;

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
