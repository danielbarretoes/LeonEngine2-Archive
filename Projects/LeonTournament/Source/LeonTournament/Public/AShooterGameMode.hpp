#pragma once

#include "Gameplay/AGameModeBase.hpp"
#include "FShooterTypes.hpp"
#include "AShooterCharacter.hpp"
#include "AShooterPlayerState.hpp"
#include "AShooterGameState.hpp"

#include <unordered_map>
#include <vector>

namespace Leon {

    class AShooterBotController;

    struct FShooterDamageCredit {
        AShooterPlayerState* Attacker = nullptr;
        float TimeSeconds = 0.0f;
        float Amount = 0.0f;
    };

    class AShooterGameMode : public AGameModeBase {
    public:
        AShooterGameMode() = default;
        AShooterGameMode(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "ShooterGameMode");

        void InitGame() override;
        void StartPlay() override;
        void Tick(float DeltaSeconds) override;

        virtual APlayerController* Login(const std::string& InPlayerName = "Player_0") override;

        AShooterGameState* GetShooterGameState() const;
        const FShooterMatchConfig& GetMatchConfig() const { return Config; }
        void SetMatchConfig(const FShooterMatchConfig& InConfig) { Config = InConfig; }

        void EnterMainMenu();
        void EnterLobby();
        void RequestStartMatch();
        void StartMatch();
        void EndMatch(EShooterMatchWinner InWinner);
        void ReturnToMenu();

        bool ApplyAuthoritativeDamage(AShooterCharacter& InInstigator, AShooterCharacter& InTarget,
                                      const FDamageInfo& InInfo);
        void NotifyDeath(AShooterCharacter& InVictim, const FDamageInfo& InInfo);
        void RespawnCharacter(AShooterCharacter& InCharacter);

        EShooterTeam AssignTeam();
        glm::vec3 GetTeamSpawnLocation(EShooterTeam InTeam) const;
        const std::vector<glm::vec3>& GetWaypoints() const { return Waypoints; }
        const std::vector<glm::vec3>& GetCoverPoints() const { return CoverPoints; }

        bool CanDamage(const AShooterCharacter& InInstigator, const AShooterCharacter& InTarget) const;
        void FillBotsToCapacity();
        int32_t CountTeam(EShooterTeam InTeam) const;

        std::vector<AShooterPlayerState*> GetSortedScoreboard() const;

        void BuildArena();
        void EnsurePlayableLighting();

    private:
        void RefreshTeamCounts();
        void TickMatch(float DeltaSeconds);
        AShooterBotController* SpawnBot(EShooterTeam InTeam, const std::string& InName);
        void PossessHumanPawns();
        bool ShouldFillBotsOnEnterLobby() const;
        bool IsCombatAllowed() const;
        void ValidateSpawnedCharacter(AShooterCharacter& InCharacter, EShooterTeam InTeam);

        FShooterMatchConfig Config;
        float StartingRemaining = 0.0f;
        bool bArenaBuilt = false;
        std::vector<glm::vec3> Waypoints;
        std::vector<glm::vec3> CoverPoints;
        std::vector<glm::vec3> Team1Spawns;
        std::vector<glm::vec3> Team2Spawns;
        std::unordered_map<AShooterCharacter*, std::vector<FShooterDamageCredit>> DamageLog;
        std::unordered_map<AShooterCharacter*, float> RespawnTimers;
        int32_t NextBotId = 0;
        mutable int32_t NextTeam1Spawn = 0;
        mutable int32_t NextTeam2Spawn = 0;
    };

} // namespace Leon
