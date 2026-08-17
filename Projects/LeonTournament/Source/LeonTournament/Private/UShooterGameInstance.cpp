#include "UShooterGameInstance.hpp"
#include "Engine/UWorld.hpp"
#include "FENetTransport.hpp"

#include <memory>

namespace Leon {

    UShooterGameInstance::UShooterGameInstance(const std::string& InName) : UGameInstance(InName) {}

    bool UShooterGameInstance::HostLan(UWorld* InWorld) {
        // Flow: host LAN session
        // 1. Remember session mode on the GameInstance (survives UI, not match scores).
        // 2. Bind ENet transport and attach the driver to the current world.
        SessionMode = EShooterSessionMode::LanHost;
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

    bool UShooterGameInstance::JoinLan(UWorld* InWorld, const std::string& InAddress) {
        SessionMode = EShooterSessionMode::LanClient;
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

    void UShooterGameInstance::ShutdownSession() {
        if (SessionNetDriver) {
            SessionNetDriver->Close();
            SessionNetDriver.reset();
        }
        SessionMode = EShooterSessionMode::Offline;
        SetNetMode(ENetMode::Standalone);
    }

    void UShooterGameInstance::Shutdown() {
        ShutdownSession();
        UGameInstance::Shutdown();
    }

} // namespace Leon
