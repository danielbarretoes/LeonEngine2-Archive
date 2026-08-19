#pragma once

#include "Gameplay/AHUD.hpp"
#include "ULeonTournamentWidgets.hpp"

namespace Leon {

    /** Fullscreen loading HUD used on the transition map. */
    class ALeonTournamentTransitionHUD : public AHUD {
    public:
        ALeonTournamentTransitionHUD() = default;
        ALeonTournamentTransitionHUD(entt::entity InHandle, UWorld* InWorld,
                                     const std::string& InName = "LeonTournamentTransitionHUD");

        void BeginPlay() override;
    };

} // namespace Leon
