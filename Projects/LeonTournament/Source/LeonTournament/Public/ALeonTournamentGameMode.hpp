#pragma once

#include "Gameplay/AGameModeBase.hpp"
#include "Gameplay/AController.hpp"
#include "FLeonTournamentTypes.hpp"
#include "ALeonTournamentCharacter.hpp"
#include "ALeonTournamentPlayerState.hpp"
#include "ALeonTournamentGameState.hpp"

#include <unordered_map>
#include <vector>

namespace Leon {

    class ALeonTournamentBotController;

    struct FLeonTournamentDamageCredit {
        ALeonTournamentPlayerState* Attacker = nullptr;
        float TimeSeconds = 0.0f;
        float Amount = 0.0f;
    };

    class ALeonTournamentGameMode : public AGameModeBase {
    public:
        ALeonTournamentGameMode() = default;
        ALeonTournamentGameMode(entt::entity InHandle, UWorld* InWorld,
                                const std::string& InName = "LeonTournamentGameMode");

        void InitGame() override;
        void StartPlay() override;
        void Tick(float DeltaSeconds) override;
        void EndPlay() override;

        virtual APlayerController* Login(const std::string& InPlayerName = "Player_0") override;
        void RestartPlayer(AController* NewPlayer) override;

        ALeonTournamentGameState* GetGameState() const;
        const FLeonTournamentMatchConfig& GetMatchConfig() const { return Config; }
        void SetMatchConfig(const FLeonTournamentMatchConfig& InConfig) { Config = InConfig; }

        void EnterMainMenu();
        void EnterLobby();
        void RequestStartMatch();
        void OpenAnimLab();
        void StartMatch();
        void EndMatch(ELeonTournamentMatchWinner InWinner);
        void ReturnToMenu();

        bool ApplyAuthoritativeDamage(ALeonTournamentCharacter& InInstigator, ALeonTournamentCharacter& InTarget,
                                      const FDamageInfo& InInfo);
        void NotifyDeath(ALeonTournamentCharacter& InVictim, const FDamageInfo& InInfo);
        void RespawnCharacter(ALeonTournamentCharacter& InCharacter);

        ELeonTournamentTeam AssignTeam();
        glm::vec3 GetTeamSpawnLocation(ELeonTournamentTeam InTeam) const;
        const std::vector<glm::vec3>& GetWaypoints() const { return Waypoints; }
        const std::vector<glm::vec3>& GetCoverPoints() const { return CoverPoints; }

        bool CanDamage(const ALeonTournamentCharacter& InInstigator, const ALeonTournamentCharacter& InTarget) const;
        void FillBotsToCapacity();
        int32_t CountTeam(ELeonTournamentTeam InTeam) const;

        std::vector<ALeonTournamentPlayerState*> GetSortedScoreboard() const;

        void BuildArena();
        void EnsurePlayableLighting();

    private:
        void RefreshTeamCounts();
        void TickMatch(float DeltaSeconds);
        void TickAutoPlay(float DeltaSeconds);
        void WriteAutoPlayReport();
        ALeonTournamentBotController* SpawnBot(ELeonTournamentTeam InTeam, const std::string& InName);
        void PossessHumanPawns();
        bool ShouldFillBotsOnEnterLobby() const;
        bool IsCombatAllowed() const;
        void ValidateSpawnedCharacter(ALeonTournamentCharacter& InCharacter, ELeonTournamentTeam InTeam);

        FLeonTournamentMatchConfig Config;
        float StartingRemaining = 0.0f;
        bool bArenaBuilt = false;
        std::vector<glm::vec3> Waypoints;
        std::vector<glm::vec3> CoverPoints;
        std::vector<glm::vec3> Team1Spawns;
        std::vector<glm::vec3> Team2Spawns;
        std::unordered_map<ALeonTournamentCharacter*, std::vector<FLeonTournamentDamageCredit>> DamageLog;
        std::unordered_map<AController*, float> RespawnTimers;
        int32_t NextBotId = 0;
        mutable int32_t NextTeam1Spawn = 0;
        mutable int32_t NextTeam2Spawn = 0;
        float AutoPlayElapsed = 0.0f;
        float AutoPlayMsSum = 0.0f;
        float AutoPlayMsMin = 1.0e9f;
        float AutoPlayMsMax = 0.0f;
        int32_t AutoPlaySamples = 0;
        bool bAutoPlayFinished = false;
    };

} // namespace Leon
