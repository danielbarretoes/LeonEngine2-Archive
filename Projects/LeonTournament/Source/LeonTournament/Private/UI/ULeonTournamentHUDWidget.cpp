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

    void ULeonTournamentHUDWidget::Build() {
        FUIRenderer::Init();
        Root = std::make_shared<UCanvasPanel>("HudRoot");
        Root->SetSize({1280, 720});

        const float topBarH = MeasurePadded("00:00", kFsTimer).y + MeasurePadded("TEAM DEATHMATCH", kFsBody).y + 18.0f;
        TopBar = std::make_shared<UImage>("TopBar");
        TopBar->SetTintColor({0.02f, 0.03f, 0.06f, 0.55f});
        Root->AddChild(TopBar, FAnchors::TopLeft(), BoxTL(0.0f, 0.0f, 1280.0f, topBarH));

        const float vitalH = MeasurePadded("999", kFsVital).y;
        const float labelH = MeasurePadded("HEALTH", kFsCaption).y;
        const float barH = 10.0f;
        const float bottomPad = 18.0f;
        const float bottomBarH = labelH + vitalH + barH + 36.0f;
        const float bottomBarW = 320.0f;

        BottomBarL = std::make_shared<UImage>("BottomBarL");
        BottomBarL->SetTintColor({0.02f, 0.04f, 0.05f, 0.62f});
        Root->AddChild(BottomBarL, FAnchors::BottomLeft(), BoxBL(24.0f, bottomPad, bottomBarW, bottomBarH));

        BottomBarR = std::make_shared<UImage>("BottomBarR");
        BottomBarR->SetTintColor({0.02f, 0.04f, 0.05f, 0.62f});
        Root->AddChild(BottomBarR, FAnchors::BottomRight(), BoxBR(24.0f, bottomPad, bottomBarW, bottomBarH));

        MatchLabel = std::make_shared<UTextBlock>("MatchLabel");
        MatchLabel->SetText("TEAM DEATHMATCH");
        MatchLabel->SetFontScale(kFsBody);
        MatchLabel->SetColor({0.72f, 0.80f, 0.92f, 0.85f});
        MatchLabel->SetJustification(ETextAlignment::Center);
        PlaceTextTC(*Root, MatchLabel, 10.0f, 280.0f);

        const float scoreTop = 10.0f + MeasurePadded("TEAM DEATHMATCH", kFsBody).y + 4.0f;

        Team1Text = std::make_shared<UTextBlock>("T1");
        Team1Text->SetText("0");
        Team1Text->SetFontScale(kFsScore);
        Team1Text->SetColor({1.0f, 0.42f, 0.32f, 1.0f});
        Team1Text->SetJustification(ETextAlignment::Right);
        PlaceTextTC(*Root, Team1Text, scoreTop, 80.0f, -140.0f);

        TimerText = std::make_shared<UTextBlock>("Timer");
        TimerText->SetText("00:00");
        TimerText->SetFontScale(kFsTimer);
        TimerText->SetColor({0.98f, 0.98f, 1.0f, 1.0f});
        TimerText->SetJustification(ETextAlignment::Center);
        PlaceTextTC(*Root, TimerText, scoreTop, 140.0f);

        Team2Text = std::make_shared<UTextBlock>("T2");
        Team2Text->SetText("0");
        Team2Text->SetFontScale(kFsScore);
        Team2Text->SetColor({0.38f, 0.62f, 1.0f, 1.0f});
        Team2Text->SetJustification(ETextAlignment::Left);
        PlaceTextTC(*Root, Team2Text, scoreTop, 80.0f, 140.0f);

        CrosshairText = std::make_shared<UTextBlock>("Crosshair");
        CrosshairText->SetText("");
        CrosshairText->SetFontScale(kFsLabel);
        CrosshairText->SetColor({0.95f, 0.97f, 1.0f, 0.0f});
        CrosshairText->SetJustification(ETextAlignment::Center);
        Root->AddChild(CrosshairText, FAnchors::Center(), BoxC(0.0f, 0.0f, 4.0f, 4.0f));

        auto centerDot = std::make_shared<UImage>("CrosshairDot");
        centerDot->SetTintColor({0.95f, 0.97f, 1.0f, 0.95f});
        Root->AddChild(centerDot, FAnchors::Center(), BoxC(0.0f, 0.0f, 3.0f, 3.0f));

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
        HealthLabel->SetFontScale(kFsCaption);
        HealthLabel->SetColor({0.55f, 0.95f, 0.65f, 0.85f});
        PlaceTextBL(*Root, HealthLabel, 44.0f, bottomPad + vitalH + barH + 14.0f, 120.0f);

        HealthText = std::make_shared<UTextBlock>("HP");
        HealthText->SetText("100");
        HealthText->SetFontScale(kFsVital);
        HealthText->SetColor({0.45f, 1.0f, 0.55f, 1.0f});
        PlaceTextBL(*Root, HealthText, 44.0f, bottomPad + barH + 12.0f, 160.0f);

        HealthBar = std::make_shared<UProgressBar>("HPBar");
        HealthBar->SetSize({240.0f, barH});
        HealthBar->SetFillColor({0.35f, 0.90f, 0.45f, 0.95f});
        FLeonTournamentUILayout::PlaceWidgetBL(*Root, HealthBar, 44.0f, bottomPad + 8.0f);

        AmmoLabel = std::make_shared<UTextBlock>("AmmoLabel");
        AmmoLabel->SetText("AMMO");
        AmmoLabel->SetFontScale(kFsCaption);
        AmmoLabel->SetColor({0.85f, 0.88f, 0.95f, 0.85f});
        AmmoLabel->SetJustification(ETextAlignment::Right);
        PlaceTextBR(*Root, AmmoLabel, 44.0f, bottomPad + vitalH + barH + 14.0f, 160.0f);

        AmmoText = std::make_shared<UTextBlock>("Ammo");
        AmmoText->SetText("30 / 30");
        AmmoText->SetFontScale(kFsScore);
        AmmoText->SetColor({0.95f, 0.97f, 1.0f, 1.0f});
        AmmoText->SetJustification(ETextAlignment::Right);
        PlaceTextBR(*Root, AmmoText, 44.0f, bottomPad + barH + 12.0f, 200.0f);

        AmmoBar = std::make_shared<UProgressBar>("AmmoBar");
        AmmoBar->SetSize({240.0f, barH});
        AmmoBar->SetFillColor({0.75f, 0.82f, 1.0f, 0.95f});
        FLeonTournamentUILayout::PlaceWidgetBR(*Root, AmmoBar, 44.0f, bottomPad + 8.0f);

        WeaponSlotsText = std::make_shared<UTextBlock>("WeaponSlots");
        WeaponSlotsText->SetText("[1]  2  3  4  5  6");
        WeaponSlotsText->SetFontScale(kFsBody);
        WeaponSlotsText->SetColor({0.78f, 0.86f, 1.0f, 0.95f});
        WeaponSlotsText->SetJustification(ETextAlignment::Right);
        PlaceTextBR(*Root, WeaponSlotsText, 44.0f, bottomPad + bottomBarH + 8.0f, 280.0f);

        StatusText = std::make_shared<UTextBlock>("Status");
        StatusText->SetText("RESPAWNING  -  FREE LOOK");
        StatusText->SetFontScale(kFsLabel);
        StatusText->SetColor({1.0f, 0.55f, 0.45f, 1.0f});
        StatusText->SetJustification(ETextAlignment::Center);
        StatusText->SetVisibility(ESlateVisibility::Collapsed);
        PlaceTextBC(*Root, StatusText, bottomPad + bottomBarH + 8.0f, 420.0f);

        KillText = std::make_shared<UTextBlock>("KillConfirm");
        KillText->SetText("KILL");
        KillText->SetFontScale(kFsHero);
        KillText->SetColor({1.0f, 0.82f, 0.25f, 1.0f});
        KillText->SetJustification(ETextAlignment::Center);
        PlaceTextC(*Root, KillText, 0.0f, -120.0f, 280.0f);
        KillText->SetVisibility(ESlateVisibility::Collapsed);

        BannerText = std::make_shared<UTextBlock>("Banner");
        BannerText->SetText("FIGHT!");
        BannerText->SetFontScale(kFsBanner);
        BannerText->SetColor({1.0f, 0.95f, 0.75f, 1.0f});
        BannerText->SetJustification(ETextAlignment::Center);
        BannerText->SetVisibility(ESlateVisibility::Collapsed);
        PlaceTextC(*Root, BannerText, 0.0f, 0.0f, 420.0f);

        DamageFlash = std::make_shared<UImage>("DamageFlash");
        DamageFlash->SetTintColor({0.85f, 0.08f, 0.08f, 0.22f});
        Root->AddChild(DamageFlash, FAnchors::Fill(), FMargin(0, 0, 0, 0));
        DamageFlash->SetVisibility(ESlateVisibility::Collapsed);

        HintText = std::make_shared<UTextBlock>("Hints");
        HintText->SetFontScale(kFsCaption);
        HintText->SetColor({0.78f, 0.86f, 0.96f, 0.92f});
        HintText->SetVisibility(ESlateVisibility::Collapsed);
        HintText->SetText(
            "V Camera   LMB Fire   RMB Scope   R Reload   Alt/C Dodge   Space x2 Double Jump   1-6 Weapons");
        PlaceTextTL(*Root, HintText, 36.0f, topBarH + 12.0f, 900.0f);

        SetWidgetTree(Root);
        SetSize({1280, 720});
        SetVisibility(ESlateVisibility::HitTestInvisible);
    }

    void ULeonTournamentHUDWidget::Tick(float InDeltaTime) {
        UUserWidget::Tick(InDeltaTime);
        auto* gs = GS(OwningPlayer);
        UWorld* world = OwningPlayer ? OwningPlayer->GetWorld() : nullptr;
        auto* animLab = world ? dynamic_cast<ALeonTournamentAnimLabGameMode*>(world->GetGameMode()) : nullptr;
        const bool bLab = animLab != nullptr;

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
                        MatchLabel->SetText("TEAM DEATHMATCH");
                }
                Team1Text->SetText(std::to_string(gs->GetTeam1Kills()));
                Team2Text->SetText(std::to_string(gs->GetTeam2Kills()));
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
                StatusText->SetText("RESPAWNING  -  FREE LOOK");
            } else if (gs && gs->GetMatchState() == ELeonTournamentMatchState::Starting) {
                StatusText->SetVisibility(ESlateVisibility::HitTestInvisible);
                StatusText->SetText("ROUND STARTING");
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
                }
            } else {
                LastCountdownSecond = -1;
                bPlayedFightBanner = false;
            }
        }

        if (HintText) {
            if (bLab) {
                HintText->SetVisibility(ESlateVisibility::HitTestInvisible);
                HintText->SetText(
                    "V Camera   LMB Fire   RMB Scope   R Reload   Alt/C Dodge   Space x2 Double Jump   1-6 Weapons");
            } else {
                HintText->SetVisibility(ESlateVisibility::Collapsed);
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
        float gap = 4.0f;
        if (tanHalfFov > 1e-4f)
            gap = std::tan(glm::radians(std::max(0.0f, spreadDeg))) / tanHalfFov * hudHalfH;
        gap = std::clamp(gap, 2.0f, hudHalfH * 0.45f);
        constexpr float barLen = 12.0f;
        constexpr float barThick = 2.0f;
        const auto center = FAnchors::Center();
        const bool bCircle =
            ch && ch->GetWeapon() &&
            ch->GetWeapon()->GetConfig().CrosshairStyle == ELeonTournamentCrosshairStyle::Circle;
        if (Root) {
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
            constexpr float segHalf = 3.5f;
            constexpr float segThick = 2.0f;
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
        constexpr float hitLen = 9.0f;
        constexpr float hitThick = 2.5f;
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
        const auto barVis = (!bCircle) ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
        const auto ringVis = bCircle ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
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
        if (HitMarkTL)
            HitMarkTL->SetVisibility(hitVis);
        if (HitMarkTR)
            HitMarkTR->SetVisibility(hitVis);
        if (HitMarkBL)
            HitMarkBL->SetVisibility(hitVis);
        if (HitMarkBR)
            HitMarkBR->SetVisibility(hitVis);
        if (CrosshairText) {
            CrosshairText->SetVisibility(ESlateVisibility::HitTestInvisible);
            CrosshairText->SetText(bHit ? "X" : "");
            CrosshairText->SetFontScale(bHit ? 1.35f : 1.0f);
            CrosshairText->SetColor(bHit ? glm::vec4(1.0f, 0.85f, 0.15f, 1.0f) : glm::vec4(0.95f, 0.97f, 1.0f, 0.0f));
            if (Root) {
                if (bHit)
                    Root->SetChildLayout(CrosshairText, center, FMargin(-10.0f, -12.0f, -10.0f, -8.0f));
                else
                    Root->SetChildLayout(CrosshairText, center, FMargin(-2.0f, -2.0f, -2.0f, -2.0f));
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
