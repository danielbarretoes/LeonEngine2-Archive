#include "Engine/UGameInstance.hpp"
#include "Engine/UWorld.hpp"

#include <memory>

namespace Leon {

    void UGameInstance::Shutdown() {
        ShutdownNetDriver();
    }

    bool UGameInstance::StartListenServer(UWorld* InWorld, uint16_t InPort) {
        // Flow: listen session
        // 1. Tear down any previous driver
        // 2. Create transport via factory (plugin, never compiled into Engine)
        // 3. Listen and attach to the world
        ShutdownNetDriver();
        auto transport = UIpNetDriver::CreateTransport();
        if (!transport)
            return false;
        SessionNetDriver = MakeRef<UIpNetDriver>();
        SessionNetDriver->SetTransport(std::move(transport));
        if (!SessionNetDriver->Listen(InPort)) {
            SessionNetDriver.reset();
            return false;
        }
        SetNetMode(ENetMode::ListenServer);
        if (InWorld) {
            World = std::static_pointer_cast<UWorld>(InWorld->shared_from_this());
            InWorld->SetNetMode(ENetMode::ListenServer);
            InWorld->SetNetDriver(SessionNetDriver.get());
            SessionNetDriver->SetWorld(InWorld);
        }
        return true;
    }

    bool UGameInstance::ConnectToHost(UWorld* InWorld, const std::string& InAddress, uint16_t InPort) {
        ShutdownNetDriver();
        auto transport = UIpNetDriver::CreateTransport();
        if (!transport)
            return false;
        SessionNetDriver = MakeRef<UIpNetDriver>();
        SessionNetDriver->SetTransport(std::move(transport));
        if (!SessionNetDriver->Connect(InAddress, InPort)) {
            SessionNetDriver.reset();
            return false;
        }
        SetNetMode(ENetMode::Client);
        if (InWorld) {
            World = std::static_pointer_cast<UWorld>(InWorld->shared_from_this());
            InWorld->SetNetMode(ENetMode::Client);
            InWorld->SetNetDriver(SessionNetDriver.get());
            SessionNetDriver->SetWorld(InWorld);
            InWorld->SetGameMode(nullptr);
        }
        return true;
    }

    void UGameInstance::ShutdownNetDriver() {
        UWorld* sessionWorld = SessionNetDriver ? SessionNetDriver->GetWorld() : nullptr;
        if (!sessionWorld && World)
            sessionWorld = World.get();
        if (SessionNetDriver) {
            if (sessionWorld && sessionWorld->GetNetDriver() == SessionNetDriver.get())
                sessionWorld->SetNetDriver(nullptr);
            SessionNetDriver->Close();
            SessionNetDriver.reset();
        }
        SetNetMode(ENetMode::Standalone);
        if (sessionWorld)
            sessionWorld->SetNetMode(ENetMode::Standalone);
        if (World && World.get() != sessionWorld)
            World->SetNetMode(ENetMode::Standalone);
    }

} // namespace Leon
