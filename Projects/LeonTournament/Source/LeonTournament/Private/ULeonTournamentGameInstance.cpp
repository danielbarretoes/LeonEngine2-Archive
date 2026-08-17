#include "ULeonTournamentGameInstance.hpp"
#include "Engine/UWorld.hpp"
#include "FENetTransport.hpp"

#include <memory>

namespace Leon {

    ULeonTournamentGameInstance::ULeonTournamentGameInstance(const std::string& InName) : UGameInstance(InName) {}

    bool ULeonTournamentGameInstance::HostLan(UWorld* InWorld) {
        // Flow: host LAN session
        // 1. Remember session mode on the GameInstance (survives UI, not match scores).
        // 2. Bind ENet transport and attach the driver to the current world.
        SessionMode = ELeonTournamentSessionMode::LanHost;
        SetNetMode(ENetMode::ListenServer);
        if (!SessionNetDriver)
            SessionNetDriver = CreateRef<UIpNetDriver>();
        if (!SessionNetDriver->GetTransport())
            SessionNetDriver->SetTransport(std::make_unique<FENetTransport>());
        if (!SessionNetDriver->Listen(LanPort))
            return false;
        if (InWorld) {
            InWorld->SetNetMode(ENetMode::ListenServer);
            InWorld->SetNetDriver(SessionNetDriver.get());
            SessionNetDriver->SetWorld(InWorld);
        }
        return true;
    }

    bool ULeonTournamentGameInstance::JoinLan(UWorld* InWorld, const std::string& InAddress) {
        SessionMode = ELeonTournamentSessionMode::LanClient;
        JoinAddress = InAddress.empty() ? JoinAddress : InAddress;
        SetNetMode(ENetMode::Client);
        if (!SessionNetDriver)
            SessionNetDriver = CreateRef<UIpNetDriver>();
        if (!SessionNetDriver->GetTransport())
            SessionNetDriver->SetTransport(std::make_unique<FENetTransport>());
        if (!SessionNetDriver->Connect(JoinAddress, LanPort))
            return false;
        if (InWorld) {
            InWorld->SetNetMode(ENetMode::Client);
            InWorld->SetNetDriver(SessionNetDriver.get());
            SessionNetDriver->SetWorld(InWorld);
        }
        return true;
    }

    void ULeonTournamentGameInstance::ShutdownSession() {
        if (SessionNetDriver) {
            SessionNetDriver->Close();
            SessionNetDriver.reset();
        }
        SessionMode = ELeonTournamentSessionMode::Offline;
        SetNetMode(ENetMode::Standalone);
    }

    void ULeonTournamentGameInstance::ConfigureAutoOfflineMatch(float InSeconds, const std::string& InReportPath) {
        bAutoOfflineMatch = true;
        SessionMode = ELeonTournamentSessionMode::Offline;
        AutoMatchSeconds = InSeconds > 1.0f ? InSeconds : 65.0f;
        AutoReportPath = InReportPath;
    }

    void ULeonTournamentGameInstance::Shutdown() {
        ShutdownSession();
        UGameInstance::Shutdown();
    }

} // namespace Leon
