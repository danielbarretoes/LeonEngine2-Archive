#include "ULeonTournamentGameInstance.hpp"
#include "FLeonTournamentGraphicsQuality.hpp"
#include "Engine/UWorld.hpp"
#include "FENetTransport.hpp"
#include "Assets/FAssetPath.hpp"
#include "Core/FConfigFile.hpp"
#include "Core/FProjectPaths.hpp"

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

    void ULeonTournamentGameInstance::Init() {
        UGameInstance::Init();
        auto world = GetWorld();
        if (!world)
            return;

        FConfigFile config;
        const std::string iniPath = FAssetPath::Combine(FProjectPaths::ProjectConfigDir(), "DefaultEngine.ini");
        const bool bLoaded = !iniPath.empty() && config.Load(iniPath);
        if (bLoaded && config.HasKey("/Script/Engine.RendererSettings", "GraphicsQuality")) {
            GraphicsQuality = FLeonTournamentGraphicsQuality::Parse(
                config.GetString("/Script/Engine.RendererSettings", "GraphicsQuality", "High"));
            FLeonTournamentGraphicsQuality::ApplyToWorld(*world, GraphicsQuality);
            return;
        }
        GraphicsQuality = FLeonTournamentGraphicsQuality::InferFromWorld(*world);
    }

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
        SessionMode = ELeonTournamentSessionMode::LanHost;
        return StartListenServer(InWorld, LanPort);
    }

    bool ULeonTournamentGameInstance::JoinLan(UWorld* InWorld, const std::string& InAddress) {
        SessionMode = ELeonTournamentSessionMode::LanClient;
        JoinAddress = InAddress.empty() ? JoinAddress : InAddress;
        return ConnectToHost(InWorld, JoinAddress, LanPort);
    }

    void ULeonTournamentGameInstance::ShutdownSession() {
        ShutdownNetDriver();
        SessionMode = ELeonTournamentSessionMode::Offline;
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
