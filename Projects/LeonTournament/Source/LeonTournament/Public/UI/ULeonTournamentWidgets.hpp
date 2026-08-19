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
#include "UMG/UTableView.hpp"
#include "UMG/ULoadingSpinner.hpp"
#include "FLeonTournamentTypes.hpp"
#include "Engine/FGraphicsQuality.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace Leon {

    class ULeonTournamentMainMenuWidget : public UUserWidget {
    public:
        ULeonTournamentMainMenuWidget(const std::string& InName = "LeonTournamentMainMenu");
        void Construct() override;
        void Tick(float InDeltaTime) override;

    private:
        void Build();
        void ApplyViewportLayout();
        void RefreshCharacterLabel();
        void NotifyCharacterCycled();
        void OnOffline();
        void OnAnimLab();
        void OnRenderLab();
        void OnHostLan();
        void OnJoinLan();
        void OnQuit();
        void OnOpenSettings();
        void OnCloseSettings();
        void OnSelectGraphicsQuality(EGraphicsQuality InQuality);
        void SetSettingsPageVisible(bool bInSettings);
        void RefreshSettingsQualityButtons();
        uint32_t ViewportWidth() const;
        uint32_t ViewportHeight() const;
        void OnPrevCharacter();
        void OnNextCharacter();
        TRef<UCanvasPanel> Root;
        TRef<UImage> Panel;
        TRef<UButton> PrevCharBtn;
        TRef<UButton> NextCharBtn;
        TRef<UEditableText> AddressField;
        TRef<UTextBlock> CharacterLabel;
        TRef<UTextBlock> Subtitle;
        TRef<UTextBlock> SettingsHint;
        TRef<UButton> SettingsLowBtn;
        TRef<UButton> SettingsMediumBtn;
        TRef<UButton> SettingsHighBtn;
        TRef<UButton> SettingsBackBtn;
        std::vector<TRef<UWidget>> MainPageWidgets;
        std::vector<TRef<UWidget>> SettingsPageWidgets;
        glm::vec2 AppliedViewport{0.0f, 0.0f};
        float AppliedLayoutScale = 0.0f;
        float DesignContentHeight = 0.0f;
        bool bInSettings = false;
        bool bPadAWasDown = false;
        bool bPadStartWasDown = false;
        bool bPadBWasDown = false;
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
        void ApplyViewportLayout();
        void RefreshCharacterLabel();
        void NotifyCharacterCycled();
        void OnStart();
        void OnBack();
        void OnPrevCharacter();
        void OnNextCharacter();
        void OnPrevMap();
        void OnNextMap();
        void OnPrevGameMode();
        void OnNextGameMode();
        void OnPrevBotDifficulty();
        void OnNextBotDifficulty();
        void OnAdjustBotsTeam1(int InDelta);
        void OnAdjustBotsTeam2(int InDelta);
        void RefreshBotLabels();
        void RefreshMapLabel();
        void RefreshGameModeLabel();
        void RefreshBotDifficultyLabel();
        TRef<UCanvasPanel> Root;
        TRef<UImage> Panel;
        TRef<UButton> PrevCharBtn;
        TRef<UButton> NextCharBtn;
        TRef<UScrollBox> RosterScroll;
        TRef<UTextBlock> RosterText;
        TRef<UTextBlock> CharacterLabel;
        glm::vec2 AppliedViewport{0.0f, 0.0f};
        float AppliedLayoutScale = 0.0f;
        float DesignContentHeight = 0.0f;
        TRef<UTextBlock> MapLabel;
        TRef<UTextBlock> GameModeLabel;
        TRef<UTextBlock> BotDifficultyLabel;
        TRef<UTextBlock> TitleText;
        TRef<UTextBlock> BotsTeam1Label;
        TRef<UTextBlock> BotsTeam2Label;
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
        bool IsCrosshairVisible() const {
            return IsVisible() &&
                   ((CrosshairImage && CrosshairImage->IsVisible()) ||
                    (CrosshairText && CrosshairText->IsVisible()));
        }
        const std::string& GetCrosshairGlyph() const {
            static const std::string empty;
            return CrosshairText ? CrosshairText->GetText() : empty;
        }

    private:
        void Build();
        void ApplyViewportLayout(float InScale);
        glm::vec2 ResolveViewportSize();
        TRef<UCanvasPanel> Root;
        TRef<UImage> TopBar;
        TRef<UImage> TopAccent;
        TRef<UImage> BottomBarL;
        TRef<UImage> BottomAccentL;
        TRef<UImage> BottomBarR;
        TRef<UImage> BottomAccentR;
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
        TRef<UImage> CrosshairImage;
        TRef<UImage> CrosshairDot;
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
        TRef<UTextBlock> DodgeCooldownText;
        std::array<TRef<UTextBlock>, 8> KillFeedLines{};
        std::array<TRef<UImage>, 8> DamageIndicators{};
        TRef<UImage> DamageFlash;
        int LastCountdownSecond = -1;
        bool bPlayedFightBanner = false;
        float AppliedLayoutScale = 0.0f;
        glm::vec2 AppliedViewport{0.0f, 0.0f};
        ELeonTournamentWeaponId LastCrosshairWeaponId = ELeonTournamentWeaponId::Count;
    };

    class ULeonTournamentLoadingOverlayWidget : public UUserWidget {
    public:
        ULeonTournamentLoadingOverlayWidget(const std::string& InName = "LeonTournamentLoadingOverlay");
        void Construct() override;
        void Tick(float InDeltaTime) override;
        void SetStatusText(const std::string& InText);

    private:
        void Build();
        void ApplyViewportLayout();
        TRef<UCanvasPanel> Root;
        TRef<ULoadingSpinner> Spinner;
        TRef<UTextBlock> StatusText;
        float AppliedLayoutScale = 0.0f;
        glm::vec2 AppliedViewport{0.0f, 0.0f};
    };

    class ULeonTournamentScoreboardWidget : public UUserWidget {
    public:
        ULeonTournamentScoreboardWidget(const std::string& InName = "LeonTournamentScoreboard");
        void Construct() override;
        void Tick(float InDeltaTime) override;

    private:
        void Build();
        void ApplyViewportLayout();
        TRef<UCanvasPanel> Root;
        TRef<UImage> Panel;
        TRef<UTextBlock> TitleText;
        TRef<UTableView> ScoreTable;
        TRef<UTextBlock> FooterText;
        float AppliedLayoutScale = 0.0f;
        glm::vec2 AppliedViewport{0.0f, 0.0f};
    };

    class ULeonTournamentPauseWidget : public UUserWidget {
    public:
        ULeonTournamentPauseWidget(const std::string& InName = "LeonTournamentPause");
        void Construct() override;
        void Tick(float InDeltaTime) override;

    private:
        void Build();
        void ApplyViewportLayout();
        void OnResume();
        void OnLeave();
        TRef<UCanvasPanel> Root;
        float AppliedLayoutScale = 0.0f;
        glm::vec2 AppliedViewport{0.0f, 0.0f};
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
        void ApplyViewportLayout();
        void OnReturn();
        void OnRematch();
        TRef<UCanvasPanel> Root;
        TRef<UTextBlock> ResultText;
        TRef<UTextBlock> StatsText;
        TRef<UButton> RematchButton;
        float AppliedLayoutScale = 0.0f;
        glm::vec2 AppliedViewport{0.0f, 0.0f};
        bool bPadAWasDown = false;
        bool bPadStartWasDown = false;
        bool bPadBWasDown = false;
    };

    /** Fixed-camera graphics lab overlay: presets + per-category knobs. */
    class ULeonTournamentRenderLabWidget : public UUserWidget {
    public:
        ULeonTournamentRenderLabWidget(const std::string& InName = "LeonTournamentRenderLab");
        void Construct() override;
        void Tick(float InDeltaTime) override;

    private:
        void Build();
        void ApplyViewportLayout();
        void RefreshLabels();
        void RefreshDebugStatus();
        void ApplyPreset(EGraphicsQuality InQuality);
        void CycleShadowResolution();
        void CycleCascadeCount();
        void CycleShadowDistance();
        void CycleShadowFilter();
        void CyclePlanarQuality();
        void TogglePlanar();
        void ToggleSSAO();
        void ToggleBloom();
        void ToggleFXAA();
        void OnCycleCamera();
        void OnBack();
        void SyncGiQualityFromWorld();
        void CommitLabKnobs();
        FGraphicsPreset ReadCurrent() const;

        TRef<UCanvasPanel> Root;
        TRef<UImage> Panel;
        TRef<UImage> DebugPanel;
        TRef<UTextBlock> StatusText;
        TRef<UTextBlock> DebugTitle;
        TRef<UTextBlock> DebugViewLine;
        std::array<TRef<UTextBlock>, 12> DebugKeyLines;
        TRef<UButton> PresetLowBtn;
        TRef<UButton> PresetMedBtn;
        TRef<UButton> PresetHighBtn;
        TRef<UButton> ShadowResBtn;
        TRef<UButton> CascadeBtn;
        TRef<UButton> ShadowDistBtn;
        TRef<UButton> ShadowFilterBtn;
        TRef<UButton> PlanarToggleBtn;
        TRef<UButton> PlanarQualityBtn;
        TRef<UButton> SsaoBtn;
        TRef<UButton> BloomBtn;
        TRef<UButton> FxaaBtn;
        TRef<UButton> CameraBtn;
        TRef<UButton> BackBtn;
        float AppliedLayoutScale = 0.0f;
        glm::vec2 AppliedViewport{0.0f, 0.0f};
        bool bPadBWasDown = false;
    };

} // namespace Leon
