#include "ULeonTournamentWidgets.hpp"
#include "FLeonTournamentUILayout.hpp"
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
#include "UMG/UProgressBar.hpp"
#include "FLeonTournamentCrosshairTextures.hpp"
#include "FLeonTournamentWeaponPresets.hpp"
#include "Renderer/FPerspectiveCamera.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <glm/glm.hpp>

namespace Leon {

    namespace {
        ULeonTournamentGameInstance* GI() { return FLeonTournamentUILayout::GI(); }
        bool GamepadEdge(int InButton, bool& InOutWasDown) { return FLeonTournamentUILayout::GamepadEdge(InButton, InOutWasDown); }
        ALeonTournamentGameMode* GM(APlayerController* InPC) { return FLeonTournamentUILayout::GM(InPC); }
        ALeonTournamentGameState* GS(APlayerController* InPC) { return FLeonTournamentUILayout::GS(InPC); }
        bool IsClientWorld(APlayerController* InPC) { return FLeonTournamentUILayout::IsClientWorld(InPC); }
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

        FMargin BoxTL(float InX, float InY, float InW, float InH) { return FLeonTournamentUILayout::BoxTL(InX, InY, InW, InH); }
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
        FMargin BoxTR(float InRight, float InTop, float InW, float InH) {
            return FMargin(-(InRight + InW), InTop, InRight, -(InTop + InH));
        }
        FMargin BoxC(float InOx, float InOy, float InW, float InH) { return FLeonTournamentUILayout::BoxC(InOx, InOy, InW, InH); }
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

    ULeonTournamentHUDWidget::ULeonTournamentHUDWidget(const std::string& InName) : UUserWidget(InName) {}
    void ULeonTournamentHUDWidget::Construct() {
        Build();
    }

    glm::vec2 ULeonTournamentHUDWidget::ResolveViewportSize() {
        if (FApplication::HasInstance()) {
            const auto& window = FApplication::Get().GetWindow();
            if (window.GetWidth() > 0 && window.GetHeight() > 0)
                return {static_cast<float>(window.GetWidth()), static_cast<float>(window.GetHeight())};
        }
        if (Root && Root->GetSize().x > 1.0f && Root->GetSize().y > 1.0f)
            return Root->GetSize();
        return {FUILayout::kDesignWidth, FUILayout::kDesignHeight};
    }

    void ULeonTournamentHUDWidget::ApplyViewportLayout(float InScale) {
        if (!Root)
            return;
        const float s = InScale;
        AppliedLayoutScale = s;
        const glm::vec2 vp = ResolveViewportSize();
        AppliedViewport = vp;
        Root->SetSize(vp);
        SetSize(vp);

        auto place = [&](const TRef<UWidget>& widget, const FAnchors& anchors, const FMargin& offsets) {
            if (!widget)
                return;
            if (!Root->SetChildLayout(widget, anchors, offsets))
                Root->AddChild(widget, anchors, offsets);
        };

        if (MatchLabel)
            MatchLabel->SetFontScale(kFsBody * s);
        if (Team1Text)
            Team1Text->SetFontScale(kFsScore * s);
        if (Team2Text)
            Team2Text->SetFontScale(kFsScore * s);
        if (TimerText)
            TimerText->SetFontScale(kFsTimer * s);
        if (HealthLabel)
            HealthLabel->SetFontScale(kFsCaption * s);
        if (HealthText)
            HealthText->SetFontScale(kFsVital * s);
        if (AmmoLabel)
            AmmoLabel->SetFontScale(kFsCaption * s);
        if (AmmoText)
            AmmoText->SetFontScale(kFsScore * s);
        if (WeaponSlotsText)
            WeaponSlotsText->SetFontScale(kFsBody * s);
        if (StatusText)
            StatusText->SetFontScale(kFsLabel * s);
        if (KillText)
            KillText->SetFontScale(kFsHero * s);
        if (BannerText)
            BannerText->SetFontScale(kFsBanner * s);
        if (HintText)
            HintText->SetFontScale(kFsCaption * s);
        if (DodgeCooldownText)
            DodgeCooldownText->SetFontScale(kFsCaption * s);
        for (auto& line : KillFeedLines) {
            if (line)
                line->SetFontScale(kFsCaption * s);
        }

        const float topBarH =
            MeasurePadded("00:00", kFsTimer * s).y + MeasurePadded("TEAM DEATHMATCH", kFsBody * s).y + 18.0f * s;
        const float vitalH = MeasurePadded("999", kFsVital * s).y;
        const float labelH = MeasurePadded("HEALTH", kFsCaption * s).y;
        const float barH = 10.0f * s;
        const float bottomPad = 18.0f * s;
        const float bottomBarH = labelH + vitalH + barH + 36.0f * s;
        const float bottomBarW = 320.0f * s;
        const float inset = 24.0f * s;
        const float textInset = 44.0f * s;
        const float scoreTop = 10.0f * s + MeasurePadded("TEAM DEATHMATCH", kFsBody * s).y + 4.0f * s;

        place(TopBar, FAnchors::TopStretch(), FUILayout::BoxTopStretch(0.0f, topBarH));
        place(BottomBarL, FAnchors::BottomLeft(), BoxBL(inset, bottomPad, bottomBarW, bottomBarH));
        place(BottomBarR, FAnchors::BottomRight(), BoxBR(inset, bottomPad, bottomBarW, bottomBarH));

        if (MatchLabel) {
            const glm::vec2 e = MeasurePadded(MatchLabel->GetText(), MatchLabel->GetFontScale());
            place(MatchLabel, FAnchors::TopCenter(), BoxTC(10.0f * s, std::max(280.0f * s, e.x), e.y));
        }
        if (Team1Text) {
            const glm::vec2 e = MeasurePadded(Team1Text->GetText(), Team1Text->GetFontScale());
            place(Team1Text, FAnchors::TopCenter(), BoxTC(scoreTop, std::max(80.0f * s, e.x), e.y, -140.0f * s));
        }
        if (TimerText) {
            const glm::vec2 e = MeasurePadded(TimerText->GetText(), TimerText->GetFontScale());
            place(TimerText, FAnchors::TopCenter(), BoxTC(scoreTop, std::max(140.0f * s, e.x), e.y));
        }
        if (Team2Text) {
            const glm::vec2 e = MeasurePadded(Team2Text->GetText(), Team2Text->GetFontScale());
            place(Team2Text, FAnchors::TopCenter(), BoxTC(scoreTop, std::max(80.0f * s, e.x), e.y, 140.0f * s));
        }

        place(CrosshairText, FAnchors::Center(), BoxC(0.0f, 0.0f, 4.0f * s, 4.0f * s));

        if (HealthLabel) {
            const glm::vec2 e = MeasurePadded(HealthLabel->GetText(), HealthLabel->GetFontScale());
            place(HealthLabel, FAnchors::BottomLeft(),
                  BoxBL(textInset, bottomPad + vitalH + barH + 14.0f * s, std::max(120.0f * s, e.x), e.y));
        }
        if (HealthText) {
            const glm::vec2 e = MeasurePadded(HealthText->GetText(), HealthText->GetFontScale());
            place(HealthText, FAnchors::BottomLeft(),
                  BoxBL(textInset, bottomPad + barH + 12.0f * s, std::max(160.0f * s, e.x), e.y));
        }
        if (HealthBar) {
            HealthBar->SetSize({240.0f * s, barH});
            place(HealthBar, FAnchors::BottomLeft(), BoxBL(textInset, bottomPad + 8.0f * s, 240.0f * s, barH));
        }
        if (AmmoLabel) {
            const glm::vec2 e = MeasurePadded(AmmoLabel->GetText(), AmmoLabel->GetFontScale());
            place(AmmoLabel, FAnchors::BottomRight(),
                  BoxBR(textInset, bottomPad + vitalH + barH + 14.0f * s, std::max(160.0f * s, e.x), e.y));
        }
        if (AmmoText) {
            const glm::vec2 e = MeasurePadded(AmmoText->GetText(), AmmoText->GetFontScale());
            place(AmmoText, FAnchors::BottomRight(),
                  BoxBR(textInset, bottomPad + barH + 12.0f * s, std::max(200.0f * s, e.x), e.y));
        }
        if (AmmoBar) {
            AmmoBar->SetSize({240.0f * s, barH});
            place(AmmoBar, FAnchors::BottomRight(), BoxBR(textInset, bottomPad + 8.0f * s, 240.0f * s, barH));
        }
        if (WeaponSlotsText) {
            const glm::vec2 e = MeasurePadded(WeaponSlotsText->GetText(), WeaponSlotsText->GetFontScale());
            place(WeaponSlotsText, FAnchors::BottomRight(),
                  BoxBR(textInset, bottomPad + bottomBarH + 8.0f * s, std::max(280.0f * s, e.x), e.y));
        }
        if (StatusText) {
            const glm::vec2 e = MeasurePadded(StatusText->GetText(), StatusText->GetFontScale());
            place(StatusText, FAnchors::BottomCenter(),
                  BoxBC(bottomPad + bottomBarH + 8.0f * s, std::max(420.0f * s, e.x), e.y));
        }
        if (KillText) {
            const glm::vec2 e = MeasurePadded(KillText->GetText(), KillText->GetFontScale());
            place(KillText, FAnchors::Center(), BoxC(0.0f, -120.0f * s, std::max(280.0f * s, e.x), e.y));
        }
        if (BannerText) {
            const glm::vec2 e = MeasurePadded(BannerText->GetText(), BannerText->GetFontScale());
            place(BannerText, FAnchors::Center(), BoxC(0.0f, 0.0f, std::max(420.0f * s, e.x), e.y));
        }
        place(DamageFlash, FAnchors::Fill(), FMargin(0.0f, 0.0f, 0.0f, 0.0f));
        if (HintText) {
            const glm::vec2 e = MeasurePadded(HintText->GetText(), HintText->GetFontScale());
            place(HintText, FAnchors::TopLeft(), BoxTL(36.0f * s, topBarH + 12.0f * s, std::max(900.0f * s, e.x), e.y));
        }
        if (DodgeCooldownText) {
            const glm::vec2 e = MeasurePadded(DodgeCooldownText->GetText(), DodgeCooldownText->GetFontScale());
            place(DodgeCooldownText, FAnchors::BottomLeft(),
                  BoxBL(textInset, bottomPad + bottomBarH + 8.0f * s, std::max(180.0f * s, e.x), e.y));
        }
        for (size_t i = 0; i < KillFeedLines.size(); ++i) {
            if (!KillFeedLines[i])
                continue;
            const glm::vec2 e = MeasurePadded(KillFeedLines[i]->GetText(), KillFeedLines[i]->GetFontScale());
            place(KillFeedLines[i], FAnchors::TopRight(),
                  BoxTR(24.0f * s, scoreTop + static_cast<float>(i) * (e.y + 4.0f * s),
                        std::max(320.0f * s, e.x), e.y));
        }
        const float edge = std::min(Root ? Root->GetSize().x : 1280.0f, Root ? Root->GetSize().y : 720.0f) * 0.42f;
        for (size_t i = 0; i < DamageIndicators.size(); ++i) {
            if (!DamageIndicators[i])
                continue;
            const float ang = static_cast<float>(i) * 0.78539816f;
            const float cx = std::cos(ang) * edge;
            const float cy = std::sin(ang) * edge;
            place(DamageIndicators[i], FAnchors::Center(),
                  BoxC(cx, cy, 18.0f * s, 6.0f * s));
        }
        if (CrosshairDot) {
            const float d = 3.0f * s;
            place(CrosshairDot, FAnchors::Center(), BoxC(0.0f, 0.0f, d, d));
        }
    }

    void ULeonTournamentHUDWidget::Build() {
        FUIRenderer::Init();
        Root = std::make_shared<UCanvasPanel>("HudRoot");
        Root->SetSize({FUILayout::kDesignWidth, FUILayout::kDesignHeight});

        TopBar = std::make_shared<UImage>("TopBar");
        TopBar->SetTintColor({0.02f, 0.03f, 0.06f, 0.55f});

        BottomBarL = std::make_shared<UImage>("BottomBarL");
        BottomBarL->SetTintColor({0.02f, 0.04f, 0.05f, 0.62f});
        BottomBarR = std::make_shared<UImage>("BottomBarR");
        BottomBarR->SetTintColor({0.02f, 0.04f, 0.05f, 0.62f});

        MatchLabel = std::make_shared<UTextBlock>("MatchLabel");
        MatchLabel->SetText("TEAM DEATHMATCH");
        MatchLabel->SetColor({0.72f, 0.80f, 0.92f, 0.85f});
        MatchLabel->SetJustification(ETextAlignment::Center);

        Team1Text = std::make_shared<UTextBlock>("T1");
        Team1Text->SetText("0");
        Team1Text->SetColor({1.0f, 0.42f, 0.32f, 1.0f});
        Team1Text->SetJustification(ETextAlignment::Right);

        TimerText = std::make_shared<UTextBlock>("Timer");
        TimerText->SetText("00:00");
        TimerText->SetColor({0.98f, 0.98f, 1.0f, 1.0f});
        TimerText->SetJustification(ETextAlignment::Center);

        Team2Text = std::make_shared<UTextBlock>("T2");
        Team2Text->SetText("0");
        Team2Text->SetColor({0.38f, 0.62f, 1.0f, 1.0f});
        Team2Text->SetJustification(ETextAlignment::Left);

        CrosshairText = std::make_shared<UTextBlock>("Crosshair");
        CrosshairText->SetText("");
        CrosshairText->SetColor({0.95f, 0.97f, 1.0f, 0.0f});
        CrosshairText->SetJustification(ETextAlignment::Center);

        CrosshairImage = std::make_shared<UImage>("CrosshairImage");
        CrosshairImage->SetVisibility(ESlateVisibility::HitTestInvisible);
        LeonTournamentApplyCrosshairBrush(*CrosshairImage, ELeonTournamentWeaponId::Rifle,
                                          LeonTournamentWeaponPreset(ELeonTournamentWeaponId::Rifle));
        CrosshairImage->SetTintColor({0.91f, 0.93f, 0.95f, 0.95f});
        Root->AddChild(CrosshairImage, FAnchors::Center(), BoxC(0.0f, 0.0f, 40.0f, 40.0f));

        CrosshairDot = std::make_shared<UImage>("CrosshairDot");
        CrosshairDot->SetTintColor({0.95f, 0.97f, 1.0f, 0.95f});
        CrosshairDot->SetVisibility(ESlateVisibility::Collapsed);
        Root->AddChild(CrosshairDot, FAnchors::Center(), BoxC(0.0f, 0.0f, 3.0f, 3.0f));

        auto makeBar = [&](const char* name) {
            auto bar = std::make_shared<UImage>(name);
            bar->SetTintColor({0.95f, 0.97f, 1.0f, 0.9f});
            Root->AddChild(bar, FAnchors::Center(), BoxC(0.0f, 0.0f, 1.0f, 1.0f));
            return bar;
        };
        CrosshairBarT = makeBar("CH_T");
        CrosshairBarB = makeBar("CH_B");
        CrosshairBarL = makeBar("CH_L");
        CrosshairBarR = makeBar("CH_R");

        for (int i = 0; i < 8; ++i) {
            auto seg = std::make_shared<UImage>(std::string("CH_Ring_") + std::to_string(i));
            seg->SetTintColor({0.95f, 0.97f, 1.0f, 0.9f});
            seg->SetVisibility(ESlateVisibility::Collapsed);
            Root->AddChild(seg, FAnchors::Center(), BoxC(0.0f, 0.0f, 1.0f, 1.0f));
            CrosshairRing[static_cast<size_t>(i)] = seg;
        }

        auto makeHit = [&](const char* name) {
            auto mark = std::make_shared<UImage>(name);
            mark->SetTintColor({1.0f, 0.85f, 0.15f, 1.0f});
            mark->SetVisibility(ESlateVisibility::Collapsed);
            Root->AddChild(mark, FAnchors::Center(), BoxC(0.0f, 0.0f, 1.0f, 1.0f));
            return mark;
        };
        HitMarkTL = makeHit("HM_TL");
        HitMarkTR = makeHit("HM_TR");
        HitMarkBL = makeHit("HM_BL");
        HitMarkBR = makeHit("HM_BR");

        HealthLabel = std::make_shared<UTextBlock>("HPLabel");
        HealthLabel->SetText("HEALTH");
        HealthLabel->SetColor({0.55f, 0.95f, 0.65f, 0.85f});

        HealthText = std::make_shared<UTextBlock>("HP");
        HealthText->SetText("100");
        HealthText->SetColor({0.45f, 1.0f, 0.55f, 1.0f});

        HealthBar = std::make_shared<UProgressBar>("HPBar");
        HealthBar->SetFillColor({0.35f, 0.90f, 0.45f, 0.95f});

        AmmoLabel = std::make_shared<UTextBlock>("AmmoLabel");
        AmmoLabel->SetText("AMMO");
        AmmoLabel->SetColor({0.85f, 0.88f, 0.95f, 0.85f});
        AmmoLabel->SetJustification(ETextAlignment::Right);

        AmmoText = std::make_shared<UTextBlock>("Ammo");
        AmmoText->SetText("30 / 30");
        AmmoText->SetColor({0.95f, 0.97f, 1.0f, 1.0f});
        AmmoText->SetJustification(ETextAlignment::Right);

        AmmoBar = std::make_shared<UProgressBar>("AmmoBar");
        AmmoBar->SetFillColor({0.75f, 0.82f, 1.0f, 0.95f});

        WeaponSlotsText = std::make_shared<UTextBlock>("WeaponSlots");
        WeaponSlotsText->SetText("[1]  2  3  4  5");
        WeaponSlotsText->SetColor({0.78f, 0.86f, 1.0f, 0.95f});
        WeaponSlotsText->SetJustification(ETextAlignment::Right);

        DodgeCooldownText = std::make_shared<UTextBlock>("DodgeCD");
        DodgeCooldownText->SetText("");
        DodgeCooldownText->SetColor({0.72f, 0.82f, 0.95f, 0.85f});
        DodgeCooldownText->SetVisibility(ESlateVisibility::Collapsed);

        StatusText = std::make_shared<UTextBlock>("Status");
        StatusText->SetText("RESPAWN IN 1.0s");
        StatusText->SetColor({1.0f, 0.55f, 0.45f, 1.0f});
        StatusText->SetJustification(ETextAlignment::Center);
        StatusText->SetVisibility(ESlateVisibility::Collapsed);

        KillText = std::make_shared<UTextBlock>("KillConfirm");
        KillText->SetText("KILL");
        KillText->SetColor({1.0f, 0.82f, 0.25f, 1.0f});
        KillText->SetJustification(ETextAlignment::Center);
        KillText->SetVisibility(ESlateVisibility::Collapsed);

        BannerText = std::make_shared<UTextBlock>("Banner");
        BannerText->SetText("FIGHT!");
        BannerText->SetColor({1.0f, 0.95f, 0.75f, 1.0f});
        BannerText->SetJustification(ETextAlignment::Center);
        BannerText->SetVisibility(ESlateVisibility::Collapsed);

        DamageFlash = std::make_shared<UImage>("DamageFlash");
        DamageFlash->SetTintColor({0.85f, 0.08f, 0.08f, 0.22f});
        DamageFlash->SetVisibility(ESlateVisibility::Collapsed);

        HintText = std::make_shared<UTextBlock>("Hints");
        HintText->SetColor({0.78f, 0.86f, 0.96f, 0.92f});
        HintText->SetVisibility(ESlateVisibility::Collapsed);
        HintText->SetText(
            "V Camera   LMB Fire   RMB Scope   R Reload   Alt/C Dodge   Space x2 Double Jump   Q/E or 1-5 Weapons");

        for (size_t i = 0; i < KillFeedLines.size(); ++i) {
            auto line = std::make_shared<UTextBlock>(std::string("KillFeed_") + std::to_string(i));
            line->SetColor({0.92f, 0.94f, 0.98f, 0.92f});
            line->SetJustification(ETextAlignment::Right);
            line->SetVisibility(ESlateVisibility::Collapsed);
            KillFeedLines[i] = line;
        }

        for (size_t i = 0; i < DamageIndicators.size(); ++i) {
            auto seg = std::make_shared<UImage>(std::string("DmgInd_") + std::to_string(i));
            seg->SetTintColor({0.95f, 0.15f, 0.12f, 0.0f});
            seg->SetVisibility(ESlateVisibility::Collapsed);
            DamageIndicators[i] = seg;
        }

        SetWidgetTree(Root);
        SetVisibility(ESlateVisibility::HitTestInvisible);
        const glm::vec2 vp = ResolveViewportSize();
        ApplyViewportLayout(FUILayout::LayoutScale(vp.x, vp.y));
    }

    void ULeonTournamentHUDWidget::Tick(float InDeltaTime) {
        UUserWidget::Tick(InDeltaTime);
        const glm::vec2 vp = ResolveViewportSize();
        const float layoutScale = FUILayout::LayoutScale(vp.x, vp.y);
        // Re-layout every frame so dynamic text (HP/ammo/timer) keeps measured boxes and fonts in sync
        // with the current resolution (720p vs 1080p).
        ApplyViewportLayout(layoutScale);

        auto* gs = GS(OwningPlayer);
        UWorld* world = OwningPlayer ? OwningPlayer->GetWorld() : nullptr;
        auto* animLab = world ? dynamic_cast<ALeonTournamentAnimLabGameMode*>(world->GetGameMode()) : nullptr;
        const bool bLab = animLab != nullptr;

        auto* gm = world ? dynamic_cast<ALeonTournamentGameMode*>(world->GetGameMode()) : nullptr;
        const bool bFFA = gm && gm->GetActiveGameMode() == ELeonTournamentGameModeId::FreeForAll;
        const bool bCTF = gm && gm->GetActiveGameMode() == ELeonTournamentGameModeId::CaptureTheFlag;
        auto* localPs = dynamic_cast<ALeonTournamentPlayerState*>(OwningPlayer ? OwningPlayer->GetPlayerState() : nullptr);

        if (Team1Text && Team2Text && TimerText) {
            if (bLab) {
                if (MatchLabel)
                    MatchLabel->SetText("ANIMATION LAB");
                Team1Text->SetText("LAB");
                Team2Text->SetText(animLab->PrefersThirdPerson() ? "3RD" : "1ST");
                TimerText->SetText("--:--");
            } else if (gs) {
                if (MatchLabel) {
                    if (gs->GetMatchState() == ELeonTournamentMatchState::Starting)
                        MatchLabel->SetText("GET READY");
                    else if (gs->GetMatchState() == ELeonTournamentMatchState::Finished)
                        MatchLabel->SetText("MATCH OVER");
                    else
                        MatchLabel->SetText(bFFA ? "FREE FOR ALL" : (bCTF ? "CAPTURE THE FLAG" : "TEAM DEATHMATCH"));
                }
                if (bFFA) {
                    const int localKills = localPs ? localPs->GetKills() : 0;
                    int leaderKills = localKills;
                    if (gm && gm->GetGameState()) {
                        for (APlayerState* ps : gm->GetGameState()->GetPlayerArray()) {
                            if (auto* sps = dynamic_cast<ALeonTournamentPlayerState*>(ps))
                                leaderKills = std::max(leaderKills, sps->GetKills());
                        }
                    }
                    Team1Text->SetText(std::to_string(localKills));
                    Team2Text->SetText("LEAD " + std::to_string(leaderKills));
                } else {
                    Team1Text->SetText(std::to_string(gs->GetTeam1Kills()));
                    Team2Text->SetText(std::to_string(gs->GetTeam2Kills()));
                }
                const float seconds = gs->GetMatchState() == ELeonTournamentMatchState::Starting
                                          ? gs->GetCountdownRemaining()
                                          : gs->GetRemainingTime();
                int t = static_cast<int>(std::max(0.0f, seconds));
                char buf[16];
                std::snprintf(buf, sizeof(buf), "%02d:%02d", t / 60, t % 60);
                TimerText->SetText(buf);
            }
        }

        ALeonTournamentCharacter* ch = OwningPlayer ? OwningPlayer->GetPawn<ALeonTournamentCharacter>() : nullptr;
        auto* health = ch ? ch->GetHealthComponent().get() : nullptr;
        auto* spc = dynamic_cast<ALeonTournamentPlayerController*>(OwningPlayer);
        const bool bDead = ch && ch->IsDeadFrozen();
        if (HealthText) {
            if (!ch || !health)
                HealthText->SetText("0");
            else if (health->IsDead() || bDead)
                HealthText->SetText("--");
            else
                HealthText->SetText(std::to_string(static_cast<int>(health->GetHealth())));
            if (health && !health->IsDead() && !bDead) {
                const float pct = health->GetMaxHealth() > 0.0f ? health->GetHealth() / health->GetMaxHealth() : 0.0f;
                if (pct < 0.3f)
                    HealthText->SetColor({1.0f, 0.35f, 0.3f, 1.0f});
                else if (pct < 0.6f)
                    HealthText->SetColor({1.0f, 0.85f, 0.35f, 1.0f});
                else
                    HealthText->SetColor({0.45f, 1.0f, 0.55f, 1.0f});
            } else {
                HealthText->SetColor({0.85f, 0.45f, 0.4f, 1.0f});
            }
            if (HealthBar) {
                float hpPct = 0.0f;
                if (health && !health->IsDead() && !bDead && health->GetMaxHealth() > 0.0f)
                    hpPct = health->GetHealth() / health->GetMaxHealth();
                HealthBar->SetPercent(hpPct);
                HealthBar->SetFillColor(HealthText->GetColor());
            }
        }
        if (AmmoText) {
            if (bLab && ch && !ch->GetWeapon()) {
                AmmoText->SetText("MELEE");
            } else if (!ch || !ch->GetWeapon() || bDead) {
                AmmoText->SetText("-- / --");
            } else if (ch->GetWeapon()->IsReloading()) {
                AmmoText->SetText("RELOAD");
            } else {
                AmmoText->SetText(std::to_string(ch->GetWeapon()->GetCurrentAmmo()) + " / " +
                                  std::to_string(ch->GetWeapon()->GetMagazineSize()));
            }
            if (AmmoBar) {
                float ammoPct = 0.0f;
                if (ch && ch->GetWeapon() && !bDead) {
                    const int mag = ch->GetWeapon()->GetMagazineSize();
                    if (mag > 0)
                        ammoPct = static_cast<float>(ch->GetWeapon()->GetCurrentAmmo()) / static_cast<float>(mag);
                }
                AmmoBar->SetPercent(ammoPct);
            }
        }
        if (AmmoLabel) {
            if (ch && ch->GetWeapon() && !bDead)
                AmmoLabel->SetText(LeonTournamentWeaponName(ch->GetActiveWeaponId()));
            else
                AmmoLabel->SetText("AMMO");
        }
        if (WeaponSlotsText) {
            if (!ch || bLab || bDead) {
                WeaponSlotsText->SetVisibility(ESlateVisibility::Collapsed);
            } else {
                WeaponSlotsText->SetVisibility(ESlateVisibility::HitTestInvisible);
                std::string slots;
                const int count = static_cast<int>(ELeonTournamentWeaponId::Count);
                for (int i = 0; i < count; ++i) {
                    if (i)
                        slots += "  ";
                    const auto id = static_cast<ELeonTournamentWeaponId>(i);
                    const bool owned = ch->HasWeapon(id);
                    const bool active = ch->GetActiveWeaponId() == id;
                    if (!owned)
                        slots += ".";
                    else if (active)
                        slots += "[" + std::to_string(i + 1) + "]";
                    else
                        slots += std::to_string(i + 1);
                }
                WeaponSlotsText->SetText(slots);
            }
        }
        if (StatusText) {
            if (bDead) {
                StatusText->SetVisibility(ESlateVisibility::HitTestInvisible);
                float respawn = 0.0f;
                if (gm && OwningPlayer)
                    respawn = gm->GetRespawnRemaining(OwningPlayer);
                char buf[96];
                if (spc && spc->IsSpectating() && !spc->GetSpectatorTargetName().empty()) {
                    if (respawn > 0.05f)
                        std::snprintf(buf, sizeof(buf), "SPECTATING: %s  -  Q/E CYCLE  -  RESPAWN IN %.1fs",
                                      spc->GetSpectatorTargetName().c_str(), respawn);
                    else
                        std::snprintf(buf, sizeof(buf), "SPECTATING: %s  -  Q/E CYCLE  -  RESPAWNING",
                                      spc->GetSpectatorTargetName().c_str());
                } else if (respawn > 0.05f) {
                    std::snprintf(buf, sizeof(buf), "RESPAWN IN %.1fs  -  Q/E SPECTATE", respawn);
                } else {
                    std::snprintf(buf, sizeof(buf), "RESPAWNING  -  Q/E SPECTATE");
                }
                StatusText->SetText(buf);
            } else if (gs && gs->GetMatchState() == ELeonTournamentMatchState::Starting) {
                StatusText->SetVisibility(ESlateVisibility::HitTestInvisible);
                StatusText->SetText("WARMUP  -  SCORE FROZEN");
            } else {
                StatusText->SetVisibility(ESlateVisibility::Collapsed);
            }
        }

        // Match countdown / FIGHT! banners + SFX
        if (spc && gs) {
            if (gs->GetMatchState() == ELeonTournamentMatchState::Starting) {
                bPlayedFightBanner = false;
                const int sec = static_cast<int>(std::ceil(std::max(0.0f, gs->GetCountdownRemaining())));
                if (sec > 0 && sec != LastCountdownSecond) {
                    LastCountdownSecond = sec;
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "%d", sec);
                    spc->PushBanner(buf, 0.85f, {1.0f, 0.95f, 0.85f, 1.0f});
                    UGameplayStatics::PlaySound2D("/Game/Audio/SFX_Countdown", 0.7f);
                }
            } else if (gs->GetMatchState() == ELeonTournamentMatchState::Playing) {
                if (!bPlayedFightBanner) {
                    bPlayedFightBanner = true;
                    LastCountdownSecond = -1;
                    spc->PushBanner("FIGHT!", 1.4f, {1.0f, 0.9f, 0.35f, 1.0f});
                    UGameplayStatics::PlaySound2D("/Game/Audio/SFX_MatchStart", 0.85f);
                    UGameplayStatics::PlaySound2D("/Game/Audio/SFX_Announce", 0.65f);
                }
            } else {
                LastCountdownSecond = -1;
                bPlayedFightBanner = false;
            }
        }

        if (HintText) {
            const bool bShowHints = bLab || (localPs && localPs->GetDeaths() < 3 && gs &&
                                             gs->GetMatchState() == ELeonTournamentMatchState::Playing);
            if (bShowHints) {
                HintText->SetVisibility(ESlateVisibility::HitTestInvisible);
                HintText->SetText(
                    "V Camera   LMB Fire   RMB Scope   R Reload   Alt/C Dodge   Space x2 Double Jump   Q/E or 1-5 Weapons");
            } else {
                HintText->SetVisibility(ESlateVisibility::Collapsed);
            }
        }
        if (DodgeCooldownText) {
            if (ch && !bDead && ch->GetDodgeCooldownRemaining() > 0.05f) {
                DodgeCooldownText->SetVisibility(ESlateVisibility::HitTestInvisible);
                char buf[32];
                std::snprintf(buf, sizeof(buf), "DODGE %.1fs", ch->GetDodgeCooldownRemaining());
                DodgeCooldownText->SetText(buf);
            } else {
                DodgeCooldownText->SetVisibility(ESlateVisibility::Collapsed);
            }
        }
        if (gm) {
            const auto& feed = gm->GetKillFeed();
            for (size_t i = 0; i < KillFeedLines.size(); ++i) {
                if (!KillFeedLines[i])
                    continue;
                if (i < feed.GetCount()) {
                    const auto& entry = feed.GetEntries()[i];
                    std::string line;
                    if (entry.Kind == ELeonTournamentKillFeedKind::Assist)
                        line = entry.InstigatorName + " assisted";
                    else
                        line = entry.InstigatorName + "  >  " + entry.VictimName;
                    KillFeedLines[i]->SetText(line);
                    KillFeedLines[i]->SetVisibility(ESlateVisibility::HitTestInvisible);
                } else {
                    KillFeedLines[i]->SetVisibility(ESlateVisibility::Collapsed);
                }
            }
        }
        float damageYaw = 0.0f;
        float damageRemaining = 0.0f;
        if (spc && spc->GetDamageIndicatorRemaining() > 0.0f) {
            damageYaw = spc->GetDamageIndicatorYawDeg();
            damageRemaining = spc->GetDamageIndicatorRemaining();
        } else if (ch && ch->GetDamageIndicatorRemaining() > 0.0f) {
            damageYaw = ch->GetLastDamageYawDeg();
            damageRemaining = ch->GetDamageIndicatorRemaining();
        }
        float camYaw = 0.0f;
        if (OwningPlayer) {
            FPerspectiveCamera viewCam;
            OwningPlayer->GetPlayerViewPoint(viewCam);
            camYaw = viewCam.GetYaw();
        }
        const float relYaw = damageYaw - camYaw;
        for (size_t i = 0; i < DamageIndicators.size(); ++i) {
            if (!DamageIndicators[i])
                continue;
            if (damageRemaining <= 0.0f) {
                DamageIndicators[i]->SetVisibility(ESlateVisibility::Collapsed);
                continue;
            }
            const float segAng = static_cast<float>(i) * 45.0f;
            const float diff = std::abs(std::fmod(relYaw - segAng + 540.0f, 360.0f) - 180.0f);
            const float alpha = diff < 30.0f ? std::clamp(damageRemaining / 0.35f, 0.0f, 1.0f) : 0.0f;
            if (alpha > 0.01f) {
                DamageIndicators[i]->SetVisibility(ESlateVisibility::HitTestInvisible);
                DamageIndicators[i]->SetTintColor({0.95f, 0.15f, 0.12f, alpha * 0.85f});
            } else {
                DamageIndicators[i]->SetVisibility(ESlateVisibility::Collapsed);
            }
        }
        // Crosshair half-gap in HUD pixels = angular spread projected through vertical FOV.
        float fovY = 95.0f;
        if (OwningPlayer) {
            FPerspectiveCamera viewCam;
            OwningPlayer->GetPlayerViewPoint(viewCam);
            fovY = std::max(10.0f, viewCam.GetFOV());
        }
        const float hudHalfH = Root ? Root->GetSize().y * 0.5f : 360.0f;
        float spreadDeg = (ch && ch->GetWeapon()) ? ch->GetWeapon()->GetCurrentSpreadDeg() : 0.35f;
        if (ch && ch->GetWeapon())
            spreadDeg += ch->GetWeapon()->GetConfig().PelletSpreadDeg * 0.5f;
        const float tanHalfFov = std::tan(glm::radians(fovY * 0.5f));
        float gap = 4.0f * AppliedLayoutScale;
        if (tanHalfFov > 1e-4f)
            gap = std::tan(glm::radians(std::max(0.0f, spreadDeg))) / tanHalfFov * hudHalfH;
        gap = std::clamp(gap, 2.0f * AppliedLayoutScale, hudHalfH * 0.45f);
        const float barLen = 12.0f * AppliedLayoutScale;
        const float barThick = 2.0f * AppliedLayoutScale;
        const auto center = FAnchors::Center();
        const ELeonTournamentWeaponId activeWeaponId = ch ? ch->GetActiveWeaponId() : ELeonTournamentWeaponId::Rifle;
        const FLeonTournamentWeaponConfig* weaponCfg = ch && ch->GetWeapon() ? &ch->GetWeapon()->GetConfig() : nullptr;
        if (CrosshairImage && weaponCfg && activeWeaponId != LastCrosshairWeaponId) {
            LastCrosshairWeaponId = activeWeaponId;
            LeonTournamentApplyCrosshairBrush(*CrosshairImage, activeWeaponId, *weaponCfg);
        }
        const float spreadScale = 1.0f + (gap / std::max(4.0f * AppliedLayoutScale, 1.0f)) * 0.18f;
        const float crosshairSize = 40.0f * AppliedLayoutScale * spreadScale;
        if (Root && CrosshairImage) {
            Root->SetChildLayout(CrosshairImage, center, BoxC(0.0f, 0.0f, crosshairSize, crosshairSize));
            CrosshairImage->SetTintColor({0.91f, 0.93f, 0.95f, 0.95f});
            CrosshairImage->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
        const bool bCircle =
            weaponCfg && weaponCfg->CrosshairStyle == ELeonTournamentCrosshairStyle::Circle;
        const bool bUseLegacyBars = !CrosshairImage || !CrosshairImage->HasBrushTexture();
        if (Root && bUseLegacyBars) {
            if (CrosshairBarT)
                Root->SetChildLayout(CrosshairBarT, center,
                                     FMargin(-(barLen * 0.5f), -(gap + barThick), -(barLen * 0.5f), gap));
            if (CrosshairBarB)
                Root->SetChildLayout(CrosshairBarB, center,
                                     FMargin(-(barLen * 0.5f), gap, -(barLen * 0.5f), -(gap + barThick)));
            if (CrosshairBarL)
                Root->SetChildLayout(CrosshairBarL, center,
                                     FMargin(-(gap + barThick), -(barLen * 0.5f), gap, -(barLen * 0.5f)));
            if (CrosshairBarR)
                Root->SetChildLayout(CrosshairBarR, center,
                                     FMargin(gap, -(barLen * 0.5f), -(gap + barThick), -(barLen * 0.5f)));
            const float segHalf = 3.5f * AppliedLayoutScale;
            const float segThick = 2.0f * AppliedLayoutScale;
            for (int i = 0; i < 8; ++i) {
                if (!CrosshairRing[static_cast<size_t>(i)])
                    continue;
                const float ang = static_cast<float>(i) * 0.78539816f;
                const float cx = std::cos(ang) * gap;
                const float cy = std::sin(ang) * gap;
                Root->SetChildLayout(CrosshairRing[static_cast<size_t>(i)], center,
                                     FMargin(cx - segHalf, cy - segThick, -(cx + segHalf), -(cy + segThick)));
            }
        }
        const bool bHit = spc && spc->IsHitMarkerActive();
        const bool bHeadshot = spc && spc->IsHeadshotMarkerActive();
        const glm::vec4 hitColor = bHeadshot ? glm::vec4(1.0f, 0.22f, 0.18f, 1.0f) : glm::vec4(1.0f, 0.85f, 0.15f, 1.0f);
        const float hitLen = 9.0f * AppliedLayoutScale;
        const float hitThick = 2.5f * AppliedLayoutScale;
        if (Root && HitMarkTL && HitMarkTR && HitMarkBL && HitMarkBR) {
            const float d = gap + 4.0f;
            Root->SetChildLayout(HitMarkTL, center,
                                 FMargin(-(d + hitLen), -(d + hitThick), d, d - hitThick));
            Root->SetChildLayout(HitMarkTR, center,
                                 FMargin(d, -(d + hitThick), -(d + hitLen), d - hitThick));
            Root->SetChildLayout(HitMarkBL, center,
                                 FMargin(-(d + hitLen), d - hitThick, d, -(d + hitThick)));
            Root->SetChildLayout(HitMarkBR, center,
                                 FMargin(d, d - hitThick, -(d + hitLen), -(d + hitThick)));
        }
        const auto barVis =
            (!bCircle && bUseLegacyBars) ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
        const auto ringVis = (bCircle && bUseLegacyBars) ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
        const auto hitVis = bHit ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
        if (CrosshairBarT)
            CrosshairBarT->SetVisibility(barVis);
        if (CrosshairBarB)
            CrosshairBarB->SetVisibility(barVis);
        if (CrosshairBarL)
            CrosshairBarL->SetVisibility(barVis);
        if (CrosshairBarR)
            CrosshairBarR->SetVisibility(barVis);
        for (auto& seg : CrosshairRing) {
            if (seg)
                seg->SetVisibility(ringVis);
        }
        if (HitMarkTL) {
            HitMarkTL->SetVisibility(hitVis);
            HitMarkTL->SetTintColor(hitColor);
        }
        if (HitMarkTR) {
            HitMarkTR->SetVisibility(hitVis);
            HitMarkTR->SetTintColor(hitColor);
        }
        if (HitMarkBL) {
            HitMarkBL->SetVisibility(hitVis);
            HitMarkBL->SetTintColor(hitColor);
        }
        if (HitMarkBR) {
            HitMarkBR->SetVisibility(hitVis);
            HitMarkBR->SetTintColor(hitColor);
        }
        if (CrosshairText) {
            CrosshairText->SetVisibility(ESlateVisibility::HitTestInvisible);
            CrosshairText->SetText(bHit ? "X" : "");
            CrosshairText->SetFontScale(bHit ? 1.35f * AppliedLayoutScale : 1.0f * AppliedLayoutScale);
            CrosshairText->SetColor(bHit ? hitColor : glm::vec4(0.95f, 0.97f, 1.0f, 0.0f));
            if (Root) {
                if (bHit)
                    Root->SetChildLayout(CrosshairText, center,
                                         FMargin(-10.0f * AppliedLayoutScale, -12.0f * AppliedLayoutScale,
                                                 -10.0f * AppliedLayoutScale, -8.0f * AppliedLayoutScale));
                else
                    Root->SetChildLayout(CrosshairText, center,
                                         FMargin(-2.0f * AppliedLayoutScale, -2.0f * AppliedLayoutScale,
                                                 -2.0f * AppliedLayoutScale, -2.0f * AppliedLayoutScale));
            }
        }
        if (KillText) {
            if (spc && spc->IsKillConfirmActive() && !spc->GetKillFeedText().empty()) {
                KillText->SetVisibility(ESlateVisibility::HitTestInvisible);
                KillText->SetText(spc->GetKillFeedText());
            } else {
                KillText->SetVisibility(ESlateVisibility::Collapsed);
            }
        }
        if (BannerText) {
            if (spc && spc->IsBannerActive() && !spc->GetBannerText().empty()) {
                BannerText->SetVisibility(ESlateVisibility::HitTestInvisible);
                BannerText->SetText(spc->GetBannerText());
                BannerText->SetColor(spc->GetBannerColor());
            } else {
                BannerText->SetVisibility(ESlateVisibility::Collapsed);
            }
        }
        if (DamageFlash)
            DamageFlash->SetVisibility(spc && spc->IsDamageFlashActive() ? ESlateVisibility::HitTestInvisible
                                                                         : ESlateVisibility::Collapsed);
    }


} // namespace Leon
