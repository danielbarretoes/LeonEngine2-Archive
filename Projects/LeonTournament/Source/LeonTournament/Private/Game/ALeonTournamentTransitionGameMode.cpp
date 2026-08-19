#include "ALeonTournamentTransitionGameMode.hpp"
#include "ULeonTournamentGameInstance.hpp"
#include "Engine/UEngine.hpp"
#include "Engine/UWorld.hpp"
#include "Gameplay/UGameplayStatics.hpp"

namespace Leon {

    namespace {
        constexpr float kTransitionDisplaySeconds = 0.25f;

        void SetTravelGameModeClass(const std::string& InClassName) {
            if (!UEngine::HasInstance())
                return;
            FGameModeConfig cfg = UEngine::Get().GetGameModeConfig();
            cfg.GameModeClass = InClassName;
            UEngine::Get().SetGameModeConfig(cfg);
        }
    } // namespace

    ALeonTournamentTransitionGameMode::ALeonTournamentTransitionGameMode(entt::entity InHandle, UWorld* InWorld,
                                                                           const std::string& InName)
        : AGameModeBase(InHandle, InWorld, InName) {
        SetClass("ALeonTournamentTransitionGameMode");
        DefaultPawnClass = "None";
        HUDClass = "ALeonTournamentTransitionHUD";
        GameStateClass = "AGameStateBase";
    }

    void ALeonTournamentTransitionGameMode::StartPlay() {
        AGameModeBase::StartPlay();
        if (!World || World->GetNetMode() == ENetMode::Client)
            return;
        World->GetTimerManager().SetTimer(
            DispatchTravelHandle,
            [this]() {
                if (!IsPendingKill())
                    DispatchPendingTravel();
            },
            kTransitionDisplaySeconds, false);
    }

    void ALeonTournamentTransitionGameMode::DispatchPendingTravel() {
        if (!World || !UEngine::HasInstance())
            return;
        auto* gi = dynamic_cast<ULeonTournamentGameInstance*>(UEngine::Get().GetGameInstance().get());
        if (!gi)
            return;

        FLeonTournamentPendingTravel pending;
        if (!gi->ConsumePendingTravel(pending))
            return;

        SetTravelGameModeClass(pending.GameModeClass);
        UGameplayStatics::OpenLevel(World, pending.DestinationMap);
    }

} // namespace Leon
