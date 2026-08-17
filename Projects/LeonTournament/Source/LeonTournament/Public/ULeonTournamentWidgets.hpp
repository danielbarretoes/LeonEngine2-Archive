#pragma once

#include "UMG/UUserWidget.hpp"
#include "UMG/UButton.hpp"
#include "UMG/UTextBlock.hpp"
#include "UMG/UCanvasPanel.hpp"
#include "UMG/UImage.hpp"
#include "UMG/UEditableText.hpp"
#include "FLeonTournamentTypes.hpp"

namespace Leon {

    class ULeonTournamentMainMenuWidget : public UUserWidget {
    public:
        ULeonTournamentMainMenuWidget(const std::string& InName = "LeonTournamentMainMenu");
        void Construct() override;

    private:
        void Build();
        void OnOffline();
        void OnAnimLab();
        void OnHostLan();
        void OnJoinLan();
        void OnQuit();
        TRef<UCanvasPanel> Root;
        TRef<UEditableText> AddressField;
    };

    class ULeonTournamentLobbyWidget : public UUserWidget {
    public:
        ULeonTournamentLobbyWidget(const std::string& InName = "LeonTournamentLobby");
        void Construct() override;
        void Tick(float InDeltaTime) override;

    private:
        void Build();
        void OnStart();
        void OnBack();
        TRef<UCanvasPanel> Root;
        TRef<UTextBlock> RosterText;
    };

    class ULeonTournamentHUDWidget : public UUserWidget {
    public:
        ULeonTournamentHUDWidget(const std::string& InName = "LeonTournamentHUD");
        void Construct() override;
        void Tick(float InDeltaTime) override;
        bool IsCrosshairVisible() const { return CrosshairText && CrosshairText->IsVisible() && IsVisible(); }
        const std::string& GetCrosshairGlyph() const {
            static const std::string empty;
            return CrosshairText ? CrosshairText->GetText() : empty;
        }

    private:
        void Build();
        TRef<UCanvasPanel> Root;
        TRef<UTextBlock> Team1Text;
        TRef<UTextBlock> Team2Text;
        TRef<UTextBlock> TimerText;
        TRef<UTextBlock> HealthText;
        TRef<UTextBlock> AmmoText;
        TRef<UTextBlock> CrosshairText;
        TRef<UImage> CrosshairBarT;
        TRef<UImage> CrosshairBarB;
        TRef<UImage> CrosshairBarL;
        TRef<UImage> CrosshairBarR;
        TRef<UImage> HitMarkTL;
        TRef<UImage> HitMarkTR;
        TRef<UImage> HitMarkBL;
        TRef<UImage> HitMarkBR;
        TRef<UTextBlock> KillText;
        TRef<UTextBlock> HintText;
        TRef<UImage> DamageFlash;
    };

    class ULeonTournamentScoreboardWidget : public UUserWidget {
    public:
        ULeonTournamentScoreboardWidget(const std::string& InName = "LeonTournamentScoreboard");
        void Construct() override;
        void Tick(float InDeltaTime) override;

    private:
        void Build();
        TRef<UCanvasPanel> Root;
        TRef<UTextBlock> RowsText;
    };

    class ULeonTournamentMatchEndWidget : public UUserWidget {
    public:
        ULeonTournamentMatchEndWidget(const std::string& InName = "LeonTournamentMatchEnd");
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
