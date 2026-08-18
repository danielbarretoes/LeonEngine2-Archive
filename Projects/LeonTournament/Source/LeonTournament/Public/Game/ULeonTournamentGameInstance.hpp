#pragma once

#include "Engine/UGameInstance.hpp"
#include "Engine/UIpNetDriver.hpp"
#include "FLeonTournamentTypes.hpp"

namespace Leon {

    /**
     * Session / application lifetime. Survives menu → lobby → match → menu.
     * Does not own match scores, health, ammo, or pawn state.
     */
    class ULeonTournamentGameInstance : public UGameInstance {
    public:
        ULeonTournamentGameInstance() = default;
        explicit ULeonTournamentGameInstance(const std::string& InName);

        ELeonTournamentSessionMode GetSessionMode() const { return SessionMode; }
        void SetSessionMode(ELeonTournamentSessionMode InMode) { SessionMode = InMode; }

        const std::string& GetJoinAddress() const { return JoinAddress; }
        void SetJoinAddress(const std::string& InAddress) { JoinAddress = InAddress; }

        ELeonTournamentCharacterSkin GetSelectedCharacterSkin() const { return SelectedCharacterSkin; }
        void SetSelectedCharacterSkin(ELeonTournamentCharacterSkin InSkin) {
            SelectedCharacterSkin = LeonTournamentClampCharacterSkin(InSkin);
        }
        void CycleSelectedCharacterSkin(int InDelta) {
            if (InDelta >= 0)
                SelectedCharacterSkin = LeonTournamentNextCharacterSkin(SelectedCharacterSkin);
            else
                SelectedCharacterSkin = LeonTournamentPrevCharacterSkin(SelectedCharacterSkin);
        }

        ELeonTournamentPlayableMap GetSelectedPlayableMap() const { return SelectedPlayableMap; }
        void SetSelectedPlayableMap(ELeonTournamentPlayableMap InMap) {
            SelectedPlayableMap = LeonTournamentClampPlayableMap(InMap);
        }
        void CycleSelectedPlayableMap(int InDelta) {
            if (InDelta >= 0)
                SelectedPlayableMap = LeonTournamentNextPlayableMap(SelectedPlayableMap);
            else
                SelectedPlayableMap = LeonTournamentPrevPlayableMap(SelectedPlayableMap);
        }

        ELeonTournamentGameModeId GetSelectedGameMode() const { return SelectedGameMode; }
        void SetSelectedGameMode(ELeonTournamentGameModeId InMode) {
            SelectedGameMode = LeonTournamentClampGameModeId(InMode);
        }
        void CycleSelectedGameMode(int InDelta) {
            if (InDelta >= 0)
                SelectedGameMode = LeonTournamentNextGameModeId(SelectedGameMode);
            else
                SelectedGameMode = LeonTournamentPrevGameModeId(SelectedGameMode);
        }

        bool ConsumePendingMatchStart() {
            const bool bPending = bPendingMatchStart;
            bPendingMatchStart = false;
            return bPending;
        }
        void SetPendingMatchStart(bool bPending) { bPendingMatchStart = bPending; }

        int32_t GetDesiredBotsTeam1() const { return DesiredBotsTeam1; }
        int32_t GetDesiredBotsTeam2() const { return DesiredBotsTeam2; }
        void SetDesiredBotsTeam1(int32_t InCount);
        void SetDesiredBotsTeam2(int32_t InCount);
        void AdjustDesiredBotsTeam1(int InDelta);
        void AdjustDesiredBotsTeam2(int InDelta);
        int32_t GetDesiredBotTotal() const { return DesiredBotsTeam1 + DesiredBotsTeam2; }

        uint16_t GetLanPort() const { return LanPort; }

        bool HostLan(UWorld* InWorld);
        bool JoinLan(UWorld* InWorld, const std::string& InAddress);
        void ShutdownSession();

        void ConfigureAutoOfflineMatch(float InSeconds, const std::string& InReportPath);
        bool IsAutoOfflineMatch() const { return bAutoOfflineMatch; }
        float GetAutoMatchSeconds() const { return AutoMatchSeconds; }
        const std::string& GetAutoReportPath() const { return AutoReportPath; }

        void Shutdown() override;

    private:
        ELeonTournamentSessionMode SessionMode = ELeonTournamentSessionMode::Offline;
        ELeonTournamentCharacterSkin SelectedCharacterSkin = ELeonTournamentCharacterSkin::YBot;
        ELeonTournamentPlayableMap SelectedPlayableMap = ELeonTournamentPlayableMap::Arena;
        ELeonTournamentGameModeId SelectedGameMode = ELeonTournamentGameModeId::TeamDeathmatch;
        int32_t DesiredBotsTeam1 = 2;
        int32_t DesiredBotsTeam2 = 2;
        std::string JoinAddress = "127.0.0.1";
        uint16_t LanPort = UIpNetDriver::DefaultPort;
        bool bAutoOfflineMatch = false;
        float AutoMatchSeconds = 65.0f;
        std::string AutoReportPath;
        bool bPendingMatchStart = false;
    };

} // namespace Leon
