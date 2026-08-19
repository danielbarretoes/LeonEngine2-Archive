#pragma once

#include "Gameplay/AGameMode.hpp"
#include "Gameplay/AController.hpp"
#include "FLeonTournamentTypes.hpp"
#include "FLeonTournamentKillFeed.hpp"
#include "ALeonTournamentCharacter.hpp"
#include "ALeonTournamentPlayerState.hpp"
#include "ALeonTournamentGameState.hpp"
#include "Engine/FTimerManager.hpp"

#include <unordered_map>
#include <vector>

namespace Leon {

    class ALeonTournamentBotController;

    struct FLeonTournamentDamageCredit {
        ALeonTournamentPlayerState* Attacker = nullptr;
        float TimeSeconds = 0.0f;
        float Amount = 0.0f;
    };

    class ALeonTournamentGameMode : public AGameMode {
    public:
        ALeonTournamentGameMode() = default;
        ALeonTournamentGameMode(entt::entity InHandle, UWorld* InWorld,
                                const std::string& InName = "LeonTournamentGameMode");

        void InitGame() override;
        void StartPlay() override;
        void Tick(float DeltaSeconds) override;
        void EndPlay() override;

        virtual APlayerController* Login(const std::string& InPlayerName = "Player_0") override;
        virtual bool PlayerCanRestart(AController* InPlayer) const override;
        void RestartPlayer(AController* NewPlayer) override;
        APlayerStart* ChoosePlayerStart(AController* InPlayer = nullptr) const override;

        ALeonTournamentGameState* GetGameState() const;
        const FLeonTournamentMatchConfig& GetMatchConfig() const { return Config; }
        void SetMatchConfig(const FLeonTournamentMatchConfig& InConfig) { Config = InConfig; }
        ELeonTournamentGameModeId GetActiveGameMode() const { return ActiveGameMode; }
        void SetActiveGameMode(ELeonTournamentGameModeId InMode) { ActiveGameMode = InMode; }
        float GetRespawnRemaining(AController* InController) const;
        const FLeonTournamentKillFeed& GetKillFeed() const { return KillFeed; }

        bool PrefersThirdPerson() const { return bPreferThirdPerson; }
        void SetPreferThirdPerson(bool bEnabled) { bPreferThirdPerson = bEnabled; }
        /** Menu-like labs: visible free cursor, no look capture. */
        virtual bool WantsUICursor() const { return false; }
        void ApplyCameraPreference(ALeonTournamentCharacter* InCharacter);

        void EnterMainMenu();
        void EnterLobby();
        void RequestStartMatch();
        void OpenAnimLab();
        void OpenRenderLab();
        void OpenNightArena();
        void OpenPlayableMap(ELeonTournamentPlayableMap InMap);
        void NotifySelectedCharacterChanged();
        void StartMatch() override;
        void EndMatch(ELeonTournamentMatchWinner InWinner);
        void RestartGame() override;
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
        void SyncLobbyBots();
        void ClearAllBots();
        int32_t CountTeam(ELeonTournamentTeam InTeam) const;
        int32_t CountHumans() const;
        int32_t CountBotsOnTeam(ELeonTournamentTeam InTeam) const;

        void BuildArena();
        void EnsurePlayableLighting();
        void TryApplyCachedArenaLightmaps();
        void TryBakeArenaLighting();

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
        void ApplyMatchCapacityFromLobby();
        void EnsureMenuShowcase();
        void DestroyMenuShowcase();
        void PlaceMenuShowcase(ALeonTournamentCharacter& InCharacter);
        void RefreshMenuShowcasePlacement();
        void CheckScoreLimitWin(ALeonTournamentPlayerState* InRecentKiller = nullptr);
        ALeonTournamentPlayerState* FindLeadingPlayerState() const;

        FLeonTournamentMatchConfig Config;
        ELeonTournamentGameModeId ActiveGameMode = ELeonTournamentGameModeId::TeamDeathmatch;
        FLeonTournamentKillFeed KillFeed;
        float StartingRemaining = 0.0f;
        bool bArenaBuilt = false;
        bool bArenaLightingBaked = false;
        bool bPreferThirdPerson = true;
        std::vector<glm::vec3> Waypoints;
        std::vector<glm::vec3> CoverPoints;
        std::vector<glm::vec3> Team1Spawns;
        std::vector<glm::vec3> Team2Spawns;
        std::unordered_map<ALeonTournamentCharacter*, std::vector<FLeonTournamentDamageCredit>> DamageLog;
        std::unordered_map<AController*, FTimerHandle> RespawnTimerHandles;
        FTimerHandle CountdownHandle;
        FTimerHandle PendingStartMatchHandle;
        int32_t NextBotId = 0;
        mutable int32_t NextTeam1Spawn = 0;
        mutable int32_t NextTeam2Spawn = 0;
        float AutoPlayElapsed = 0.0f;
        float AutoPlayMsSum = 0.0f;
        float AutoPlayMsMin = 1.0e9f;
        float AutoPlayMsMax = 0.0f;
        int32_t AutoPlaySamples = 0;
        bool bAutoPlayFinished = false;
        bool bAutoPlayCollectorReady = false;
        ALeonTournamentCharacter* ShowcaseCharacter = nullptr;
        AActor* ShowcaseFloor = nullptr;
    };

} // namespace Leon
