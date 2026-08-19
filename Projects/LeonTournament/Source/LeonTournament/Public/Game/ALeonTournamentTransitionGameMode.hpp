#pragma once

#include "Gameplay/AGameModeBase.hpp"
#include "Engine/FTimerManager.hpp"

namespace Leon {

    /**
     * @brief Empty transition map GameMode. Shows a loading HUD, then travels to the pending destination.
     */
    class ALeonTournamentTransitionGameMode : public AGameModeBase {
    public:
        ALeonTournamentTransitionGameMode() = default;
        ALeonTournamentTransitionGameMode(entt::entity InHandle, UWorld* InWorld,
                                          const std::string& InName = "LeonTournamentTransitionGameMode");

        void StartPlay() override;
        bool WantsUICursor() const { return false; }

    private:
        void DispatchPendingTravel();
        FTimerHandle DispatchTravelHandle;
    };

} // namespace Leon
