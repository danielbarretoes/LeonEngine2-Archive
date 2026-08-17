#pragma once

#include "Gameplay/AHUD.hpp"
#include "UShooterWidgets.hpp"

namespace Leon {

    class AShooterHUD : public AHUD {
    public:
        AShooterHUD() = default;
        AShooterHUD(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "ShooterHUD");

        void BeginPlay() override;
        void Tick(float DeltaSeconds) override;

        TRef<UShooterHUDWidget> GetHudWidget() const { return HudWidget; }
        bool IsScoreboardVisible() const { return bScoreboardVisible; }

    private:
        void SyncWidgets();

        TRef<UShooterMainMenuWidget> MenuWidget;
        TRef<UShooterLobbyWidget> LobbyWidget;
        TRef<UShooterHUDWidget> HudWidget;
        TRef<UShooterScoreboardWidget> ScoreboardWidget;
        TRef<UShooterMatchEndWidget> EndWidget;
        EShooterMatchState ShownState = EShooterMatchState::MainMenu;
        bool bScoreboardVisible = false;
    };

} // namespace Leon
