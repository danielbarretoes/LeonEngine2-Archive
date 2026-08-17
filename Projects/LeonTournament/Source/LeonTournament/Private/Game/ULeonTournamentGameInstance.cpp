#include "ULeonTournamentGameInstance.hpp"
#include "Engine/UWorld.hpp"
#include "FENetTransport.hpp"

#include <algorithm>
#include <memory>

namespace Leon {

    namespace {
        constexpr int32_t kMaxMatchSlots = 12;
        constexpr int32_t kMaxBotsPerTeam = 6;

        void ClampDesiredBots(int32_t& InOutTeam1, int32_t& InOutTeam2) {
            InOutTeam1 = std::clamp(InOutTeam1, 0, kMaxBotsPerTeam);
            InOutTeam2 = std::clamp(InOutTeam2, 0, kMaxBotsPerTeam);
            // Reserve at least one slot for a local human when possible.
            while (InOutTeam1 + InOutTeam2 > kMaxMatchSlots - 1 && (InOutTeam1 > 0 || InOutTeam2 > 0)) {
                if (InOutTeam1 >= InOutTeam2 && InOutTeam1 > 0)
                    --InOutTeam1;
                else if (InOutTeam2 > 0)
                    --InOutTeam2;
                else
                    break;
            }
        }
    } // namespace

    ULeonTournamentGameInstance::ULeonTournamentGameInstance(const std::string& InName) : UGameInstance(InName) {}

    void ULeonTournamentGameInstance::SetDesiredBotsTeam1(int32_t InCount) {
        DesiredBotsTeam1 = InCount;
        ClampDesiredBots(DesiredBotsTeam1, DesiredBotsTeam2);
    }

    void ULeonTournamentGameInstance::SetDesiredBotsTeam2(int32_t InCount) {
        DesiredBotsTeam2 = InCount;
        ClampDesiredBots(DesiredBotsTeam1, DesiredBotsTeam2);
    }

    void ULeonTournamentGameInstance::AdjustDesiredBotsTeam1(int InDelta) {
        SetDesiredBotsTeam1(DesiredBotsTeam1 + InDelta);
    }

    void ULeonTournamentGameInstance::AdjustDesiredBotsTeam2(int InDelta) {
        SetDesiredBotsTeam2(DesiredBotsTeam2 + InDelta);
    }

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
            // Clients must not keep a local GameMode as match authority.
            InWorld->SetGameMode(nullptr);
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
