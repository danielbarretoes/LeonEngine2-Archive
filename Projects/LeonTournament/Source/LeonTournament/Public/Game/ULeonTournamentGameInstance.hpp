#pragma once

#include "Engine/UGameInstance.hpp"
#include "Engine/UIpNetDriver.hpp"
#include "FLeonTournamentTypes.hpp"
#include "Engine/FGraphicsQuality.hpp"

namespace Leon {

    struct FLeonTournamentPendingTravel {
        std::string DestinationMap;
        std::string GameModeClass = "ALeonTournamentGameMode";
        std::string LoadingLabel = "LOADING...";
        bool bValid = false;
    };

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

        ELeonTournamentBotDifficulty GetBotDifficulty() const { return BotDifficulty; }
        void SetBotDifficulty(ELeonTournamentBotDifficulty InDifficulty) { BotDifficulty = InDifficulty; }
        void CycleBotDifficulty(int InDelta) {
            const uint8_t count = 3;
            uint8_t v = static_cast<uint8_t>(BotDifficulty);
            if (InDelta >= 0)
                v = static_cast<uint8_t>((v + 1) % count);
            else
                v = static_cast<uint8_t>((v + count - 1) % count);
            BotDifficulty = static_cast<ELeonTournamentBotDifficulty>(v);
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

        EGraphicsQuality GetGraphicsQuality() const { return GraphicsQuality; }
        void SetGraphicsQuality(EGraphicsQuality InQuality) { GraphicsQuality = InQuality; }

        void Init() override;
        void Shutdown() override;

        const std::string& GetTransitionMapPath() const { return TransitionMapPath; }
        void BeginTravelWithTransition(UWorld* InWorld, const std::string& InDestinationMap,
                                       const std::string& InGameModeClass, const std::string& InLoadingLabel);
        bool ConsumePendingTravel(FLeonTournamentPendingTravel& OutTravel);
        bool HasPendingTravel() const { return PendingTravel.bValid; }

        void SetLoadingOverlayActive(bool bActive, const std::string& InLabel = "LOADING...");
        bool IsLoadingOverlayActive() const { return bLoadingOverlayActive; }
        const std::string& GetLoadingOverlayLabel() const { return LoadingOverlayLabel; }

    private:
        ELeonTournamentSessionMode SessionMode = ELeonTournamentSessionMode::Offline;
        ELeonTournamentCharacterSkin SelectedCharacterSkin = ELeonTournamentCharacterSkin::YBot;
        ELeonTournamentPlayableMap SelectedPlayableMap = ELeonTournamentPlayableMap::Arena;
        ELeonTournamentGameModeId SelectedGameMode = ELeonTournamentGameModeId::TeamDeathmatch;
        ELeonTournamentBotDifficulty BotDifficulty = ELeonTournamentBotDifficulty::Normal;
        int32_t DesiredBotsTeam1 = 2;
        int32_t DesiredBotsTeam2 = 2;
        std::string JoinAddress = "127.0.0.1";
        uint16_t LanPort = UIpNetDriver::DefaultPort;
        bool bAutoOfflineMatch = false;
        float AutoMatchSeconds = 65.0f;
        std::string AutoReportPath;
        bool bPendingMatchStart = false;
        EGraphicsQuality GraphicsQuality = EGraphicsQuality::High;
        std::string TransitionMapPath = "/Game/Maps/Transition";
        FLeonTournamentPendingTravel PendingTravel;
        bool bLoadingOverlayActive = false;
        std::string LoadingOverlayLabel = "LOADING...";
    };

} // namespace Leon
