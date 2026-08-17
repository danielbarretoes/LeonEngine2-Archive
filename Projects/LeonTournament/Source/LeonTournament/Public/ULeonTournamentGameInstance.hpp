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
        std::string JoinAddress = "127.0.0.1";
        uint16_t LanPort = UIpNetDriver::DefaultPort;
        TRef<UIpNetDriver> SessionNetDriver;
        bool bAutoOfflineMatch = false;
        float AutoMatchSeconds = 65.0f;
        std::string AutoReportPath;
    };

} // namespace Leon
