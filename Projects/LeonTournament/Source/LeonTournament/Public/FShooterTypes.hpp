#pragma once

#include <cstdint>
#include <string>

namespace Leon {

    enum class EShooterTeam : uint8_t { None = 0, Team1 = 1, Team2 = 2 };

    enum class EShooterMatchState : uint8_t { MainMenu = 0, Lobby = 1, Starting = 2, Playing = 3, Finished = 4 };

    enum class EShooterMatchWinner : uint8_t { None = 0, Team1 = 1, Team2 = 2, Draw = 3 };

    enum class EShooterSessionMode : uint8_t { Offline = 0, LanHost = 1, LanClient = 2 };

    enum class EShooterBotState : uint8_t {
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

    inline const char* ShooterBotStateName(EShooterBotState InState) {
        switch (InState) {
        case EShooterBotState::Idle:
            return "IDLE";
        case EShooterBotState::Search:
            return "SEARCH";
        case EShooterBotState::MoveToTarget:
            return "MOVE";
        case EShooterBotState::Combat:
            return "COMBAT";
        case EShooterBotState::TakeCover:
            return "COVER";
        case EShooterBotState::Aim:
            return "AIM";
        case EShooterBotState::Fire:
            return "FIRE";
        case EShooterBotState::Reload:
            return "RELOAD";
        case EShooterBotState::Dead:
            return "DEAD";
        case EShooterBotState::Respawn:
            return "RESPAWN";
        default:
            return "?";
        }
    }

    inline const char* ShooterTeamName(EShooterTeam InTeam) {
        switch (InTeam) {
        case EShooterTeam::Team1:
            return "TEAM 1";
        case EShooterTeam::Team2:
            return "TEAM 2";
        default:
            return "NONE";
        }
    }

    struct FShooterMatchConfig {
        float MatchDurationSeconds = 600.0f;
        int32_t ScoreLimit = 50;
        int32_t MaxTeamSize = 8;
        int32_t MaxPlayers = 16;
        float AssistWindowSeconds = 5.0f;
        float RespawnDelaySeconds = 3.0f;
        float StartCountdownSeconds = 2.0f;
        bool bFriendlyFire = false;
    };

    struct FShooterRifleConfig {
        float FireRate = 10.0f;
        float Damage = 20.0f;
        int32_t MagazineSize = 30;
        float ReloadTime = 2.0f;
        float Range = 200.0f;
    };

    struct FShooterBotPersonality {
        float Aggression = 0.55f;
        float Accuracy = 0.6f;
        float ReactionTime = 0.22f;
        float PreferredRange = 15.0f;
        float StrafeFrequency = 1.1f;
        float CoverPreference = 0.45f;
        int32_t Tactic = 0;
    };

} // namespace Leon
