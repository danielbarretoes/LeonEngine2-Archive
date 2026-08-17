#pragma once

#include <cstdint>
#include <string>

namespace Leon {

    enum class ELeonTournamentTeam : uint8_t { None = 0, Team1 = 1, Team2 = 2 };

    enum class ELeonTournamentMatchState : uint8_t { MainMenu = 0, Lobby = 1, Starting = 2, Playing = 3, Finished = 4 };

    enum class ELeonTournamentMatchWinner : uint8_t { None = 0, Team1 = 1, Team2 = 2, Draw = 3 };

    enum class ELeonTournamentSessionMode : uint8_t { Offline = 0, LanHost = 1, LanClient = 2 };

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
        int32_t MaxTeamSize = 2;
        int32_t MaxPlayers = 4;
        float AssistWindowSeconds = 5.0f;
        float RespawnDelaySeconds = 2.0f;
        float StartCountdownSeconds = 2.0f;
        bool bFriendlyFire = false;
    };

    struct FLeonTournamentRifleConfig {
        float FireRate = 10.0f;
        float Damage = 20.0f;
        int32_t MagazineSize = 30;
        float ReloadTime = 2.0f;
        float Range = 200.0f;
    };

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
