#include "ULeonTournamentWidgets.hpp"
#include "FLeonTournamentUILayout.hpp"
#include "ALeonTournamentHUD.hpp"
#include "ALeonTournamentAnimLabGameMode.hpp"
#include "ALeonTournamentGameMode.hpp"
#include "ALeonTournamentGameState.hpp"
#include "ALeonTournamentPlayerController.hpp"
#include "ALeonTournamentCharacter.hpp"
#include "ALeonTournamentPlayerState.hpp"
#include "ALeonTournamentWeapon.hpp"
#include "ULeonTournamentGameInstance.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Gameplay/UGameplayStatics.hpp"
#include "Engine/UEngine.hpp"
#include "Engine/UWorld.hpp"
#include "Core/FApplication.hpp"
#include "Core/FInput.hpp"
#include "Core/FInputSettings.hpp"
#include "UMG/FUIRenderer.hpp"
#include "UMG/UWidgetSwitcher.hpp"
#include "UMG/UCanvasPanel.hpp"
#include "Renderer/FPerspectiveCamera.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <sstream>
#include <glm/glm.hpp>

namespace Leon {

    namespace {
        ULeonTournamentGameInstance* GI() {
            return FLeonTournamentUILayout::GI();
        }
        bool GamepadEdge(int InButton, bool& InOutWasDown) {
            return FLeonTournamentUILayout::GamepadEdge(InButton, InOutWasDown);
        }
        ALeonTournamentGameMode* GM(APlayerController* InPC) {
            return FLeonTournamentUILayout::GM(InPC);
        }
        ALeonTournamentGameState* GS(APlayerController* InPC) {
            return FLeonTournamentUILayout::GS(InPC);
        }
        bool IsClientWorld(APlayerController* InPC) {
            return FLeonTournamentUILayout::IsClientWorld(InPC);
        }
        constexpr float kFsCaption = FLeonTournamentUILayout::kFsCaption;
        constexpr float kFsBody = FLeonTournamentUILayout::kFsBody;
        constexpr float kFsLabel = FLeonTournamentUILayout::kFsLabel;
        constexpr float kFsButton = FLeonTournamentUILayout::kFsButton;
        constexpr float kFsSub = FLeonTournamentUILayout::kFsSub;
        constexpr float kFsTitle = FLeonTournamentUILayout::kFsTitle;
        constexpr float kFsHero = FLeonTournamentUILayout::kFsHero;
        constexpr float kFsScore = FLeonTournamentUILayout::kFsScore;
        constexpr float kFsTimer = FLeonTournamentUILayout::kFsTimer;
        constexpr float kFsVital = FLeonTournamentUILayout::kFsVital;
        constexpr float kFsBanner = FLeonTournamentUILayout::kFsBanner;

        FMargin BoxTL(float InX, float InY, float InW, float InH) {
            return FLeonTournamentUILayout::BoxTL(InX, InY, InW, InH);
        }
        FMargin BoxBL(float InX, float InBottom, float InW, float InH) {
            return FLeonTournamentUILayout::BoxBL(InX, InBottom, InW, InH);
        }
        FMargin BoxBR(float InRight, float InBottom, float InW, float InH) {
            return FLeonTournamentUILayout::BoxBR(InRight, InBottom, InW, InH);
        }
        FMargin BoxTC(float InTop, float InW, float InH, float InOx = 0.0f) {
            return FLeonTournamentUILayout::BoxTC(InTop, InW, InH, InOx);
        }
        FMargin BoxBC(float InBottom, float InW, float InH, float InOx = 0.0f) {
            return FLeonTournamentUILayout::BoxBC(InBottom, InW, InH, InOx);
        }
        FMargin BoxC(float InOx, float InOy, float InW, float InH) {
            return FLeonTournamentUILayout::BoxC(InOx, InOy, InW, InH);
        }
        glm::vec2 MeasurePadded(const std::string& InText, float InScale, float InPadX = 8.0f, float InPadY = 6.0f) {
            return FLeonTournamentUILayout::MeasurePadded(InText, InScale, InPadX, InPadY);
        }
        TRef<UButton> MakeButton(const std::string& InName, const std::string& InLabel, float InFont = kFsButton,
                                 float InMinW = 200.0f, float InMinH = 0.0f) {
            return FLeonTournamentUILayout::MakeButton(InName, InLabel, InFont, InMinW, InMinH);
        }
        void PlaceButtonTL(UCanvasPanel& InRoot, const TRef<UButton>& InBtn, float InX, float InY) {
            FLeonTournamentUILayout::PlaceButtonTL(InRoot, InBtn, InX, InY);
        }
        void PlaceButtonC(UCanvasPanel& InRoot, const TRef<UButton>& InBtn, float InOx, float InOy) {
            FLeonTournamentUILayout::PlaceButtonC(InRoot, InBtn, InOx, InOy);
        }
        void PlaceTextTL(UCanvasPanel& InRoot, const TRef<UTextBlock>& InText, float InX, float InY,
                         float InMinW = 0.0f, float InMinH = 0.0f) {
            FLeonTournamentUILayout::PlaceTextTL(InRoot, InText, InX, InY, InMinW, InMinH);
        }
        void PlaceTextTC(UCanvasPanel& InRoot, const TRef<UTextBlock>& InText, float InTop, float InMinW = 0.0f,
                         float InOx = 0.0f) {
            FLeonTournamentUILayout::PlaceTextTC(InRoot, InText, InTop, InMinW, InOx);
        }
        void PlaceTextBL(UCanvasPanel& InRoot, const TRef<UTextBlock>& InText, float InX, float InBottom,
                         float InMinW = 0.0f) {
            FLeonTournamentUILayout::PlaceTextBL(InRoot, InText, InX, InBottom, InMinW);
        }
        void PlaceTextBR(UCanvasPanel& InRoot, const TRef<UTextBlock>& InText, float InRight, float InBottom,
                         float InMinW = 0.0f) {
            FLeonTournamentUILayout::PlaceTextBR(InRoot, InText, InRight, InBottom, InMinW);
        }
        void PlaceTextBC(UCanvasPanel& InRoot, const TRef<UTextBlock>& InText, float InBottom, float InMinW = 0.0f) {
            FLeonTournamentUILayout::PlaceTextBC(InRoot, InText, InBottom, InMinW);
        }
        void PlaceTextC(UCanvasPanel& InRoot, const TRef<UTextBlock>& InText, float InOx, float InOy,
                        float InMinW = 0.0f, float InMinH = 0.0f) {
            FLeonTournamentUILayout::PlaceTextC(InRoot, InText, InOx, InOy, InMinW, InMinH);
        }
    } // namespace

    ULeonTournamentMainMenuWidget::ULeonTournamentMainMenuWidget(const std::string& InName) : UUserWidget(InName) {}
    void ULeonTournamentMainMenuWidget::Construct() {
        Build();
    }

    void ULeonTournamentMainMenuWidget::Build() {
        FUIRenderer::Init();
        Root = std::make_shared<UCanvasPanel>("MenuRoot");
        Root->SetSize({1280, 720});
        Root->SetBackgroundColor({0.0f, 0.0f, 0.0f, 0.0f});

        Panel = std::make_shared<UImage>("MenuPanel");
        Panel->SetTintColor({0.02f, 0.03f, 0.06f, 0.92f});
        Root->AddChild(Panel, FAnchors::LeftStretch(),
                       FUILayout::BoxLeftStretch(0.0f, kLeonTournamentMenuPanelDesignWidth));

        constexpr float leftX = 48.0f;
        float y = 36.0f;

        auto title = std::make_shared<UTextBlock>("Title");
        title->SetText("LEON TOURNAMENT");
        title->SetFontScale(kFsHero);
        title->SetColor({0.90f, 0.93f, 1.0f, 1.0f});
        PlaceTextTL(*Root, title, leftX, y);
        y += MeasurePadded(title->GetText(), title->GetFontScale()).y + 2.0f;

        Subtitle = std::make_shared<UTextBlock>("Sub");
        Subtitle->SetText("2v2 Team Deathmatch");
        Subtitle->SetFontScale(kFsBody);
        Subtitle->SetColor({0.65f, 0.72f, 0.85f, 1.0f});
        PlaceTextTL(*Root, Subtitle, leftX, y);
        y += MeasurePadded(Subtitle->GetText(), Subtitle->GetFontScale()).y + 16.0f;

        const float stackY = y;
        constexpr float btnGap = 8.0f;
        constexpr float btnMinH = 40.0f;

        auto play = MakeButton("Play", "PLAY", kFsButton, 300.0f, btnMinH);
        play->OnClicked.AddLambda([this]() { OnOffline(); });
        PlaceButtonTL(*Root, play, leftX, y);
        y += play->GetSize().y + btnGap;

        auto training = MakeButton("Training", "TRAINING", kFsButton, 300.0f, btnMinH);
        training->OnClicked.AddLambda([this]() { OnAnimLab(); });
        PlaceButtonTL(*Root, training, leftX, y);
        y += training->GetSize().y + btnGap;

        auto renderLab = MakeButton("RenderLab", "RENDER LAB", kFsButton, 300.0f, btnMinH);
        renderLab->OnClicked.AddLambda([this]() { OnRenderLab(); });
        PlaceButtonTL(*Root, renderLab, leftX, y);
        y += renderLab->GetSize().y + btnGap;

        auto host = MakeButton("Host", "HOST LAN", kFsButton, 300.0f, btnMinH);
        host->OnClicked.AddLambda([this]() { OnHostLan(); });
        PlaceButtonTL(*Root, host, leftX, y);
        y += host->GetSize().y + btnGap;

        auto join = MakeButton("Join", "JOIN LAN", kFsButton, 300.0f, btnMinH);
        join->OnClicked.AddLambda([this]() { OnJoinLan(); });
        PlaceButtonTL(*Root, join, leftX, y);
        y += join->GetSize().y + 10.0f;

        auto ipLabel = std::make_shared<UTextBlock>("IpLabel");
        ipLabel->SetText("IP Address");
        ipLabel->SetFontScale(kFsCaption);
        ipLabel->SetColor({0.65f, 0.72f, 0.85f, 1.0f});
        PlaceTextTL(*Root, ipLabel, leftX, y);
        y += MeasurePadded(ipLabel->GetText(), ipLabel->GetFontScale()).y + 2.0f;

        AddressField = std::make_shared<UEditableText>("JoinAddress");
        auto* session = GI();
        AddressField->SetText(session && !session->GetJoinAddress().empty() ? session->GetJoinAddress()
                                                                            : std::string("127.0.0.1"));
        AddressField->SetHint("127.0.0.1");
        AddressField->OnTextChanged = [](const std::string& InText) {
            if (auto* inst = GI())
                inst->SetJoinAddress(InText.empty() ? "127.0.0.1" : InText);
        };
        const float fieldH = 36.0f;
        const float fieldW = 300.0f;
        AddressField->SetSize({fieldW, fieldH});
        Root->AddChild(AddressField, FAnchors::TopLeft(), BoxTL(leftX, y, fieldW, fieldH));
        y += fieldH + 12.0f;

        auto settings = MakeButton("Settings", "SETTINGS", kFsButton, 300.0f, btnMinH);
        settings->OnClicked.AddLambda([this]() { OnOpenSettings(); });
        PlaceButtonTL(*Root, settings, leftX, y);
        y += settings->GetSize().y + btnGap;

        auto quit = MakeButton("Quit", "QUIT", kFsButton, 300.0f, btnMinH);
        quit->OnClicked.AddLambda([this]() { OnQuit(); });
        PlaceButtonTL(*Root, quit, leftX, y);
        DesignContentHeight = y + quit->GetSize().y + 8.0f;

        float settingsY = stackY;
        SettingsHint = std::make_shared<UTextBlock>("SettingsHint");
        SettingsHint->SetText("Texture limit + estimated VRAM at this resolution");
        SettingsHint->SetFontScale(kFsCaption);
        SettingsHint->SetColor({0.65f, 0.72f, 0.85f, 1.0f});
        PlaceTextTL(*Root, SettingsHint, leftX, settingsY);
        settingsY += MeasurePadded(SettingsHint->GetText(), SettingsHint->GetFontScale()).y + 10.0f;

        SettingsLowBtn = MakeButton("QualityLow", "LOW", kFsButton, 300.0f, btnMinH);
        SettingsLowBtn->OnClicked.AddLambda([this]() { OnSelectGraphicsQuality(EGraphicsQuality::Low); });
        PlaceButtonTL(*Root, SettingsLowBtn, leftX, settingsY);
        settingsY += SettingsLowBtn->GetSize().y + btnGap;

        SettingsMediumBtn = MakeButton("QualityMedium", "MEDIUM", kFsButton, 300.0f, btnMinH);
        SettingsMediumBtn->OnClicked.AddLambda(
            [this]() { OnSelectGraphicsQuality(EGraphicsQuality::Medium); });
        PlaceButtonTL(*Root, SettingsMediumBtn, leftX, settingsY);
        settingsY += SettingsMediumBtn->GetSize().y + btnGap;

        SettingsHighBtn = MakeButton("QualityHigh", "HIGH", kFsButton, 300.0f, btnMinH);
        SettingsHighBtn->OnClicked.AddLambda(
            [this]() { OnSelectGraphicsQuality(EGraphicsQuality::High); });
        PlaceButtonTL(*Root, SettingsHighBtn, leftX, settingsY);
        settingsY += SettingsHighBtn->GetSize().y + 16.0f;

        SettingsBackBtn = MakeButton("SettingsBack", "BACK", kFsButton, 300.0f, btnMinH);
        SettingsBackBtn->OnClicked.AddLambda([this]() { OnCloseSettings(); });
        PlaceButtonTL(*Root, SettingsBackBtn, leftX, settingsY);
        DesignContentHeight = std::max(DesignContentHeight, settingsY + SettingsBackBtn->GetSize().y + 8.0f);

        MainPageWidgets = {play, training, renderLab, host, join, ipLabel, AddressField, settings, quit};
        SettingsPageWidgets = {SettingsHint, SettingsLowBtn, SettingsMediumBtn, SettingsHighBtn, SettingsBackBtn};
        SetSettingsPageVisible(false);

        PrevCharBtn = MakeButton("PrevChar", "<", kFsSub, 56.0f, 52.0f);
        PrevCharBtn->OnClicked.AddLambda([this]() { OnPrevCharacter(); });

        CharacterLabel = std::make_shared<UTextBlock>("CharName");
        CharacterLabel->SetFontScale(kFsSub);
        CharacterLabel->SetColor({0.95f, 0.97f, 1.0f, 1.0f});
        CharacterLabel->SetJustification(ETextAlignment::Center);
        CharacterLabel->SetText("YBOT");
        RefreshCharacterLabel();

        NextCharBtn = MakeButton("NextChar", ">", kFsSub, 56.0f, 52.0f);
        NextCharBtn->OnClicked.AddLambda([this]() { OnNextCharacter(); });

        SetWidgetTree(Root);
        ApplyViewportLayout();
    }

    void ULeonTournamentMainMenuWidget::ApplyViewportLayout() {
        if (!Root)
            return;
        SetSize(FLeonTournamentUILayout::ResolveViewportSize(Root.get()));
        FLeonTournamentUILayout::SyncResolutionScale(*Root, AppliedLayoutScale, AppliedViewport, DesignContentHeight,
                                                     100.0f);
        FLeonTournamentUILayout::ApplyMenuRailLayout(*Root, Panel, PrevCharBtn, CharacterLabel, NextCharBtn, false,
                                                     AppliedLayoutScale);
    }

    void ULeonTournamentMainMenuWidget::Tick(float InDeltaTime) {
        UUserWidget::Tick(InDeltaTime);
        const glm::vec2 vp = FLeonTournamentUILayout::ResolveViewportSize(Root.get());
        const float scale = FUILayout::LayoutScaleFit(vp.x, vp.y, DesignContentHeight, 100.0f);
        if (std::abs(scale - AppliedLayoutScale) > 0.001f || glm::length(vp - AppliedViewport) > 1.0f) {
            ApplyViewportLayout();
            if (bInSettings)
                RefreshSettingsQualityButtons();
        }
        RefreshCharacterLabel();
        if (bInSettings) {
            if (GamepadEdge(GamepadButton::B, bPadBWasDown))
                OnCloseSettings();
            bPadAWasDown = FInput::IsGamepadButtonPressed(GamepadButton::A, FInputSettings::Get().GamepadId);
            bPadStartWasDown = FInput::IsGamepadButtonPressed(GamepadButton::Start, FInputSettings::Get().GamepadId);
        } else if (GamepadEdge(GamepadButton::A, bPadAWasDown) || GamepadEdge(GamepadButton::Start, bPadStartWasDown)) {
            OnOffline();
        }
        if (GamepadEdge(GamepadButton::LeftBumper, bPadLBWasDown))
            OnPrevCharacter();
        if (GamepadEdge(GamepadButton::RightBumper, bPadRBWasDown))
            OnNextCharacter();
    }

    void ULeonTournamentMainMenuWidget::RefreshCharacterLabel() {
        if (!CharacterLabel)
            return;
        auto* gi = GI();
        const auto skin = gi ? gi->GetSelectedCharacterSkin() : ELeonTournamentCharacterSkin::YBot;
        CharacterLabel->SetText(LeonTournamentCharacterSkinName(skin));
    }

    void ULeonTournamentMainMenuWidget::NotifyCharacterCycled() {
        RefreshCharacterLabel();
        if (auto* gm = GM(OwningPlayer))
            gm->NotifySelectedCharacterChanged();
    }

    void ULeonTournamentMainMenuWidget::OnPrevCharacter() {
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_UIClick", 0.45f);
        if (auto* gi = GI())
            gi->CycleSelectedCharacterSkin(-1);
        NotifyCharacterCycled();
    }

    void ULeonTournamentMainMenuWidget::OnNextCharacter() {
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_UIClick", 0.45f);
        if (auto* gi = GI())
            gi->CycleSelectedCharacterSkin(1);
        NotifyCharacterCycled();
    }

    void ULeonTournamentMainMenuWidget::OnOffline() {
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_UIClick", 0.5f);
        if (IsClientWorld(OwningPlayer))
            return;
        auto* hud = OwningPlayer ? dynamic_cast<ALeonTournamentHUD*>(OwningPlayer->GetHUD()) : nullptr;
        if (hud)
            hud->ShowLoadingOverlay("ENTERING LOBBY...");
        if (auto* gi = GI())
            gi->SetSessionMode(ELeonTournamentSessionMode::Offline);
        if (auto* gm = GM(OwningPlayer))
            gm->EnterLobby();
    }

    void ULeonTournamentMainMenuWidget::OnAnimLab() {
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_UIClick", 0.5f);
        if (IsClientWorld(OwningPlayer))
            return;
        if (auto* hud = OwningPlayer ? dynamic_cast<ALeonTournamentHUD*>(OwningPlayer->GetHUD()) : nullptr)
            hud->ShowLoadingOverlay("LOADING ANIM LAB...");
        if (auto* gm = GM(OwningPlayer))
            gm->OpenAnimLab();
    }

    void ULeonTournamentMainMenuWidget::OnRenderLab() {
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_UIClick", 0.5f);
        if (IsClientWorld(OwningPlayer))
            return;
        if (auto* hud = OwningPlayer ? dynamic_cast<ALeonTournamentHUD*>(OwningPlayer->GetHUD()) : nullptr)
            hud->ShowLoadingOverlay("LOADING RENDER LAB...");
        if (auto* gm = GM(OwningPlayer))
            gm->OpenRenderLab();
    }

    void ULeonTournamentMainMenuWidget::OnHostLan() {
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_UIClick", 0.5f);
        auto* hud = OwningPlayer ? dynamic_cast<ALeonTournamentHUD*>(OwningPlayer->GetHUD()) : nullptr;
        if (hud)
            hud->ShowLoadingOverlay("HOSTING...");
        if (auto* gi = GI()) {
            if (OwningPlayer && OwningPlayer->GetWorld()) {
                if (!gi->HostLan(OwningPlayer->GetWorld()) && hud)
                    hud->HideLoadingOverlay();
            }
        }
        if (auto* gm = GM(OwningPlayer))
            gm->EnterLobby();
    }

    void ULeonTournamentMainMenuWidget::OnJoinLan() {
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_UIClick", 0.5f);
        auto* hud = OwningPlayer ? dynamic_cast<ALeonTournamentHUD*>(OwningPlayer->GetHUD()) : nullptr;
        if (hud)
            hud->ShowLoadingOverlay("CONNECTING...");
        if (auto* gi = GI()) {
            if (OwningPlayer && OwningPlayer->GetWorld()) {
                if (!gi->JoinLan(OwningPlayer->GetWorld(), gi->GetJoinAddress()) && hud)
                    hud->HideLoadingOverlay();
            }
        }
    }

    void ULeonTournamentMainMenuWidget::OnQuit() {
        if (FApplication::HasInstance())
            FApplication::Get().Close();
    }

    void ULeonTournamentMainMenuWidget::SetSettingsPageVisible(bool bSettings) {
        bInSettings = bSettings;
        const auto visibilityFor = [](bool bShow) {
            return bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
        };
        for (auto& widget : MainPageWidgets) {
            if (widget)
                widget->SetVisibility(visibilityFor(!bSettings));
        }
        for (auto& widget : SettingsPageWidgets) {
            if (widget)
                widget->SetVisibility(visibilityFor(bSettings));
        }
        if (Subtitle)
            Subtitle->SetText(bSettings ? "GRAPHICS" : "2v2 Team Deathmatch");
    }

    uint32_t ULeonTournamentMainMenuWidget::ViewportWidth() const {
        return static_cast<uint32_t>(std::max(AppliedViewport.x, 1280.0f));
    }

    uint32_t ULeonTournamentMainMenuWidget::ViewportHeight() const {
        return static_cast<uint32_t>(std::max(AppliedViewport.y, 720.0f));
    }

    void ULeonTournamentMainMenuWidget::RefreshSettingsQualityButtons() {
        const uint32_t w = ViewportWidth();
        const uint32_t h = ViewportHeight();
        const auto quality = GI() ? GI()->GetGraphicsQuality() : EGraphicsQuality::High;
        const glm::vec4 selected{0.16f, 0.42f, 0.72f, 1.0f};
        const glm::vec4 idle{0.12f, 0.18f, 0.30f, 0.95f};
        auto apply = [&](const TRef<UButton>& btn, EGraphicsQuality q) {
            if (!btn)
                return;
            if (auto label = std::dynamic_pointer_cast<UTextBlock>(btn->GetContent())) {
                label->SetText(FGraphicsQuality::FormatVRAMLabel(q, w, h));
                label->SetSize(FUIRenderer::MeasureString(label->GetText(), label->GetFontScale()));
            }
            btn->SetNormalColor(q == quality ? selected : idle);
        };
        apply(SettingsLowBtn, EGraphicsQuality::Low);
        apply(SettingsMediumBtn, EGraphicsQuality::Medium);
        apply(SettingsHighBtn, EGraphicsQuality::High);
    }

    void ULeonTournamentMainMenuWidget::OnOpenSettings() {
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_UIClick", 0.5f);
        SetSettingsPageVisible(true);
        RefreshSettingsQualityButtons();
    }

    void ULeonTournamentMainMenuWidget::OnCloseSettings() {
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_UIClick", 0.45f);
        SetSettingsPageVisible(false);
    }

    void ULeonTournamentMainMenuWidget::OnSelectGraphicsQuality(EGraphicsQuality InQuality) {
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_UIClick", 0.5f);
        if (auto* gi = GI()) {
            gi->SetGraphicsQuality(InQuality);
            if (auto world = gi->GetWorld())
                FGraphicsQuality::ApplyToWorld(*world, InQuality);
        }
        FGraphicsQuality::PersistToEngineIni(InQuality);
        RefreshSettingsQualityButtons();
    }

} // namespace Leon
