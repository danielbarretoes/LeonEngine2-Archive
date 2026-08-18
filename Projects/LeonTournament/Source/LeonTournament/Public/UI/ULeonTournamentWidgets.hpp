#pragma once

#include "UMG/UUserWidget.hpp"
#include "UMG/UButton.hpp"
#include "UMG/UTextBlock.hpp"
#include "UMG/UCanvasPanel.hpp"
#include "UMG/UImage.hpp"
#include "UMG/UProgressBar.hpp"
#include "UMG/UEditableText.hpp"
#include "UMG/USlider.hpp"
#include "UMG/UCheckBox.hpp"
#include "UMG/UWidgetSwitcher.hpp"
#include "UMG/UScrollBox.hpp"
#include "FLeonTournamentTypes.hpp"

#include <array>

namespace Leon {

    class ULeonTournamentMainMenuWidget : public UUserWidget {
    public:
        ULeonTournamentMainMenuWidget(const std::string& InName = "LeonTournamentMainMenu");
        void Construct() override;
        void Tick(float InDeltaTime) override;

    private:
        void Build();
        void RefreshCharacterLabel();
        void OnOffline();
        void OnAnimLab();
        void OnHostLan();
        void OnJoinLan();
        void OnQuit();
        void OnPrevCharacter();
        void OnNextCharacter();
        TRef<UCanvasPanel> Root;
        TRef<UWidgetSwitcher> SessionSwitcher;
        TRef<UEditableText> AddressField;
        TRef<UTextBlock> CharacterLabel;
        bool bPadAWasDown = false;
        bool bPadStartWasDown = false;
        bool bPadLBWasDown = false;
        bool bPadRBWasDown = false;
    };

    class ULeonTournamentLobbyWidget : public UUserWidget {
    public:
        ULeonTournamentLobbyWidget(const std::string& InName = "LeonTournamentLobby");
        void Construct() override;
        void Tick(float InDeltaTime) override;

    private:
        void Build();
        void RefreshCharacterLabel();
        void OnStart();
        void OnBack();
        void OnPrevCharacter();
        void OnNextCharacter();
        void OnAdjustBotsTeam1(int InDelta);
        void OnAdjustBotsTeam2(int InDelta);
        void RefreshBotLabels();
        TRef<UCanvasPanel> Root;
        TRef<UScrollBox> RosterScroll;
        TRef<UTextBlock> RosterText;
        TRef<UTextBlock> CharacterLabel;
        TRef<UTextBlock> TitleText;
        TRef<UTextBlock> BotsTeam1Label;
        TRef<UTextBlock> BotsTeam2Label;
        TRef<USlider> BotsTeam1Slider;
        TRef<UTextBlock> CapacityHint;
        bool bPadAWasDown = false;
        bool bPadStartWasDown = false;
        bool bPadBWasDown = false;
        bool bPadLBWasDown = false;
        bool bPadRBWasDown = false;
        bool bPadDLeftWasDown = false;
        bool bPadDRightWasDown = false;
        bool bPadDUpWasDown = false;
        bool bPadDDownWasDown = false;
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
        TRef<UImage> TopBar;
        TRef<UImage> BottomBarL;
        TRef<UImage> BottomBarR;
        TRef<UTextBlock> MatchLabel;
        TRef<UTextBlock> Team1Text;
        TRef<UTextBlock> Team2Text;
        TRef<UTextBlock> TimerText;
        TRef<UTextBlock> HealthLabel;
        TRef<UTextBlock> HealthText;
        TRef<UProgressBar> HealthBar;
        TRef<UTextBlock> AmmoLabel;
        TRef<UTextBlock> AmmoText;
        TRef<UProgressBar> AmmoBar;
        TRef<UTextBlock> WeaponSlotsText;
        TRef<UTextBlock> StatusText;
        TRef<UTextBlock> CrosshairText;
        TRef<UImage> CrosshairBarT;
        TRef<UImage> CrosshairBarB;
        TRef<UImage> CrosshairBarL;
        TRef<UImage> CrosshairBarR;
        std::array<TRef<UImage>, 8> CrosshairRing{};
        TRef<UImage> HitMarkTL;
        TRef<UImage> HitMarkTR;
        TRef<UImage> HitMarkBL;
        TRef<UImage> HitMarkBR;
        TRef<UTextBlock> KillText;
        TRef<UTextBlock> BannerText;
        TRef<UTextBlock> HintText;
        TRef<UImage> DamageFlash;
        int LastCountdownSecond = -1;
        bool bPlayedFightBanner = false;
    };

    class ULeonTournamentScoreboardWidget : public UUserWidget {
    public:
        ULeonTournamentScoreboardWidget(const std::string& InName = "LeonTournamentScoreboard");
        void Construct() override;
        void Tick(float InDeltaTime) override;

    private:
        void Build();
        TRef<UCanvasPanel> Root;
        TRef<UImage> Panel;
        TRef<UTextBlock> TitleText;
        TRef<UTextBlock> HeaderText;
        TRef<UTextBlock> RowsText;
        TRef<UTextBlock> FooterText;
    };

    class ULeonTournamentPauseWidget : public UUserWidget {
    public:
        ULeonTournamentPauseWidget(const std::string& InName = "LeonTournamentPause");
        void Construct() override;
        void Tick(float InDeltaTime) override;

    private:
        void Build();
        void OnResume();
        void OnLeave();
        TRef<UCanvasPanel> Root;
        bool bPadAWasDown = false;
        bool bPadBWasDown = false;
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
        bool bPadAWasDown = false;
        bool bPadStartWasDown = false;
    };

} // namespace Leon
