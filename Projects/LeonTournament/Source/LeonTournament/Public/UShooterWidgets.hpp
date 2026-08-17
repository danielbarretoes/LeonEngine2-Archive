#pragma once

#include "UMG/UUserWidget.hpp"
#include "UMG/UButton.hpp"
#include "UMG/UTextBlock.hpp"
#include "UMG/UCanvasPanel.hpp"
#include "UMG/UImage.hpp"
#include "UMG/UEditableText.hpp"
#include "FShooterTypes.hpp"

namespace Leon {

    class UShooterMainMenuWidget : public UUserWidget {
    public:
        UShooterMainMenuWidget(const std::string& InName = "ShooterMainMenu");
        void Construct() override;

    private:
        void Build();
        void OnOffline();
        void OnHostLan();
        void OnJoinLan();
        void OnQuit();
        TRef<UCanvasPanel> Root;
        TRef<UEditableText> AddressField;
    };

    class UShooterLobbyWidget : public UUserWidget {
    public:
        UShooterLobbyWidget(const std::string& InName = "ShooterLobby");
        void Construct() override;
        void Tick(float InDeltaTime) override;

    private:
        void Build();
        void OnStart();
        void OnBack();
        TRef<UCanvasPanel> Root;
        TRef<UTextBlock> RosterText;
    };

    class UShooterHUDWidget : public UUserWidget {
    public:
        UShooterHUDWidget(const std::string& InName = "ShooterHUD");
        void Construct() override;
        void Tick(float InDeltaTime) override;
        bool IsCrosshairVisible() const {
            return CrossH && CrossV && CrossH->IsVisible() && CrossV->IsVisible() && IsVisible();
        }

    private:
        void Build();
        TRef<UCanvasPanel> Root;
        TRef<UTextBlock> Team1Text;
        TRef<UTextBlock> Team2Text;
        TRef<UTextBlock> TimerText;
        TRef<UTextBlock> HealthText;
        TRef<UTextBlock> AmmoText;
        TRef<UTextBlock> HitMarkText;
        TRef<UTextBlock> KillText;
        TRef<UTextBlock> SprintText;
        TRef<UImage> DamageFlash;
        TRef<UImage> CrossH;
        TRef<UImage> CrossV;
    };

    class UShooterScoreboardWidget : public UUserWidget {
    public:
        UShooterScoreboardWidget(const std::string& InName = "ShooterScoreboard");
        void Construct() override;
        void Tick(float InDeltaTime) override;

    private:
        void Build();
        TRef<UCanvasPanel> Root;
        TRef<UTextBlock> RowsText;
    };

    class UShooterMatchEndWidget : public UUserWidget {
    public:
        UShooterMatchEndWidget(const std::string& InName = "ShooterMatchEnd");
        void Construct() override;
        void Tick(float InDeltaTime) override;

    private:
        void Build();
        void OnReturn();
        TRef<UCanvasPanel> Root;
        TRef<UTextBlock> ResultText;
        TRef<UTextBlock> StatsText;
    };

} // namespace Leon
