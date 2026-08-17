#pragma once

#include "Engine/UGameInstance.hpp"
#include "Engine/UIpNetDriver.hpp"
#include "FShooterTypes.hpp"

namespace Leon {

    /**
     * Session / application lifetime. Survives menu → lobby → match → menu.
     * Does not own match scores, health, ammo, or pawn state.
     */
    class UShooterGameInstance : public UGameInstance {
    public:
        UShooterGameInstance() = default;
        explicit UShooterGameInstance(const std::string& InName);

        EShooterSessionMode GetSessionMode() const { return SessionMode; }
        void SetSessionMode(EShooterSessionMode InMode) { SessionMode = InMode; }

        const std::string& GetJoinAddress() const { return JoinAddress; }
        void SetJoinAddress(const std::string& InAddress) { JoinAddress = InAddress; }

        uint16_t GetLanPort() const { return LanPort; }

        bool HostLan(UWorld* InWorld);
        bool JoinLan(UWorld* InWorld, const std::string& InAddress);
        void ShutdownSession();

        void Shutdown() override;

    private:
        EShooterSessionMode SessionMode = EShooterSessionMode::Offline;
        std::string JoinAddress = "127.0.0.1";
        uint16_t LanPort = UIpNetDriver::DefaultPort;
        TRef<UIpNetDriver> SessionNetDriver;
    };

} // namespace Leon
