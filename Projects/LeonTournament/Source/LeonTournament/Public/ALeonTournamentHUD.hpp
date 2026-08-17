#pragma once

#include "Gameplay/AHUD.hpp"
#include "ULeonTournamentWidgets.hpp"

namespace Leon {

    class ALeonTournamentHUD : public AHUD {
    public:
        ALeonTournamentHUD() = default;
        ALeonTournamentHUD(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "LeonTournamentHUD");

        void BeginPlay() override;
        void Tick(float DeltaSeconds) override;

        TRef<ULeonTournamentHUDWidget> GetHudWidget() const { return HudWidget; }
        bool IsScoreboardVisible() const { return bScoreboardVisible; }

    private:
        void SyncWidgets();

        TRef<ULeonTournamentMainMenuWidget> MenuWidget;
        TRef<ULeonTournamentLobbyWidget> LobbyWidget;
        TRef<ULeonTournamentHUDWidget> HudWidget;
        TRef<ULeonTournamentScoreboardWidget> ScoreboardWidget;
        TRef<ULeonTournamentMatchEndWidget> EndWidget;
        ELeonTournamentMatchState ShownState = ELeonTournamentMatchState::MainMenu;
        bool bScoreboardVisible = false;
    };

} // namespace Leon
