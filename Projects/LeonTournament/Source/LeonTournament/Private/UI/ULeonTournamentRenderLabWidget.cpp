#include "ULeonTournamentWidgets.hpp"
#include "FLeonTournamentUILayout.hpp"
#include "ALeonTournamentGameMode.hpp"
#include "ALeonTournamentRenderLabGameMode.hpp"
#include "ULeonTournamentGameInstance.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Gameplay/UGameplayStatics.hpp"
#include "Engine/UEngine.hpp"
#include "Engine/UWorld.hpp"
#include "Core/FApplication.hpp"
#include "Core/FInput.hpp"
#include "Core/FInputSettings.hpp"
#include "UMG/FUIRenderer.hpp"
#include "Renderer/FShadowTypes.hpp"
#include "Renderer/FPlanarReflectionTypes.hpp"
#include "Renderer/FWorldRenderer.hpp"
#include "Renderer/FRenderDebugHotkeys.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

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
        constexpr float kFsCaption = FLeonTournamentUILayout::kFsCaption;
        constexpr float kFsBody = FLeonTournamentUILayout::kFsBody;
        constexpr float kFsButton = FLeonTournamentUILayout::kFsButton;
        constexpr float kFsTitle = FLeonTournamentUILayout::kFsTitle;

        FMargin BoxTL(float InX, float InY, float InW, float InH) {
            return FLeonTournamentUILayout::BoxTL(InX, InY, InW, InH);
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
        void PlaceTextTL(UCanvasPanel& InRoot, const TRef<UTextBlock>& InText, float InX, float InY,
                         float InMinW = 0.0f, float InMinH = 0.0f) {
            FLeonTournamentUILayout::PlaceTextTL(InRoot, InText, InX, InY, InMinW, InMinH);
        }

        void SetButtonLabel(const TRef<UButton>& InBtn, const std::string& InText) {
            if (!InBtn)
                return;
            if (auto label = std::dynamic_pointer_cast<UTextBlock>(InBtn->GetContent())) {
                label->SetText(InText);
                label->SetSize(FUIRenderer::MeasureString(InText, label->GetFontScale()));
            }
        }

        const char* FilterLabel(EShadowFilterMode InFilter) {
            switch (InFilter) {
            case EShadowFilterMode::Hard:
                return "Hard";
            case EShadowFilterMode::PCF5x5:
                return "PCF5x5";
            case EShadowFilterMode::Poisson:
                return "Poisson";
            case EShadowFilterMode::PCF3x3:
            default:
                return "PCF3x3";
            }
        }

        const char* PlanarLabel(EPlanarReflectionQuality InQuality) {
            switch (InQuality) {
            case EPlanarReflectionQuality::Low:
                return "Low";
            case EPlanarReflectionQuality::Medium:
                return "Medium";
            case EPlanarReflectionQuality::High:
                return "High";
            case EPlanarReflectionQuality::Epic:
            default:
                return "Epic";
            }
        }

        EShadowFilterMode NextFilter(EShadowFilterMode InFilter) {
            switch (InFilter) {
            case EShadowFilterMode::Hard:
                return EShadowFilterMode::PCF3x3;
            case EShadowFilterMode::PCF3x3:
                return EShadowFilterMode::PCF5x5;
            case EShadowFilterMode::PCF5x5:
                return EShadowFilterMode::Poisson;
            case EShadowFilterMode::Poisson:
            default:
                return EShadowFilterMode::Hard;
            }
        }

        EPlanarReflectionQuality NextPlanar(EPlanarReflectionQuality InQuality) {
            switch (InQuality) {
            case EPlanarReflectionQuality::Low:
                return EPlanarReflectionQuality::Medium;
            case EPlanarReflectionQuality::Medium:
                return EPlanarReflectionQuality::High;
            case EPlanarReflectionQuality::High:
                return EPlanarReflectionQuality::Epic;
            case EPlanarReflectionQuality::Epic:
            default:
                return EPlanarReflectionQuality::Low;
            }
        }
    } // namespace

    ULeonTournamentRenderLabWidget::ULeonTournamentRenderLabWidget(const std::string& InName) : UUserWidget(InName) {}

    void ULeonTournamentRenderLabWidget::Construct() {
        Build();
    }

    void ULeonTournamentRenderLabWidget::Build() {
        FUIRenderer::Init();
        Root = std::make_shared<UCanvasPanel>("RenderLabRoot");
        Root->SetSize({1280, 720});
        Root->SetBackgroundColor({0.0f, 0.0f, 0.0f, 0.0f});

        constexpr float panelW = 232.0f;
        constexpr float panelH = 548.0f;
        constexpr float designW = 1280.0f;
        Panel = std::make_shared<UImage>("RenderLabPanel");
        Panel->SetTintColor({0.02f, 0.03f, 0.06f, 0.82f});
        Root->AddChild(Panel, FAnchors::TopLeft(), BoxTL(designW - panelW - 8.0f, 8.0f, panelW, panelH));

        constexpr float debugW = 248.0f;
        constexpr float debugH = 214.0f;
        DebugPanel = std::make_shared<UImage>("RenderLabDebugPanel");
        DebugPanel->SetTintColor({0.02f, 0.03f, 0.06f, 0.80f});
        Root->AddChild(DebugPanel, FAnchors::TopLeft(), BoxTL(8.0f, 8.0f, debugW, debugH));

        constexpr float debugX = 16.0f;
        float debugY = 14.0f;
        DebugTitle = std::make_shared<UTextBlock>("RLDebugTitle");
        DebugTitle->SetText("DEBUG");
        DebugTitle->SetFontScale(kFsCaption);
        DebugTitle->SetColor({0.92f, 0.95f, 1.0f, 1.0f});
        PlaceTextTL(*Root, DebugTitle, debugX, debugY);
        debugY += MeasurePadded(DebugTitle->GetText(), DebugTitle->GetFontScale()).y;

        DebugViewLine = std::make_shared<UTextBlock>("RLDebugView");
        DebugViewLine->SetFontScale(kFsCaption);
        DebugViewLine->SetColor({0.45f, 0.92f, 1.0f, 1.0f});
        DebugViewLine->SetText("View: Lit");
        PlaceTextTL(*Root, DebugViewLine, debugX, debugY, 220.0f, 14.0f);
        debugY += 14.0f;

        const char* keyPlaceholders[12] = {
            "F1 HUD off",  "F2 Gizmos off", "F3 Wire off", "F4 Mat off", "F5 Geo off",  "F6 Light off",
            "F7 IBL off",  "F8 Shadow off", "F9 Planar off", "F10 Post off", "F11 FX on", "F12 Reset",
        };
        for (int i = 0; i < 12; ++i) {
            char name[24];
            std::snprintf(name, sizeof(name), "RLDebugF%d", i + 1);
            DebugKeyLines[static_cast<size_t>(i)] = std::make_shared<UTextBlock>(name);
            DebugKeyLines[static_cast<size_t>(i)]->SetFontScale(kFsCaption);
            DebugKeyLines[static_cast<size_t>(i)]->SetColor({0.62f, 0.68f, 0.78f, 1.0f});
            DebugKeyLines[static_cast<size_t>(i)]->SetText(keyPlaceholders[i]);
            PlaceTextTL(*Root, DebugKeyLines[static_cast<size_t>(i)], debugX, debugY, 220.0f, 13.0f);
            debugY += 13.0f;
        }

        constexpr float leftX = designW - panelW + 4.0f;
        float y = 16.0f;
        constexpr float gap = 3.0f;
        constexpr float btnW = 208.0f;
        constexpr float btnH = 24.0f;

        auto title = std::make_shared<UTextBlock>("RLTitle");
        title->SetText("RENDER LAB");
        title->SetFontScale(kFsCaption);
        title->SetColor({0.92f, 0.95f, 1.0f, 1.0f});
        PlaceTextTL(*Root, title, leftX, y);
        y += MeasurePadded(title->GetText(), title->GetFontScale()).y + 2.0f;

        auto hint = std::make_shared<UTextBlock>("RLHint");
        hint->SetText("F-keys left. ESC=menu");
        hint->SetFontScale(kFsCaption);
        hint->SetColor({0.65f, 0.72f, 0.85f, 1.0f});
        PlaceTextTL(*Root, hint, leftX, y);
        y += MeasurePadded(hint->GetText(), hint->GetFontScale()).y + 6.0f;

        CameraBtn = MakeButton("RLCamera", "CAM A", kFsCaption, btnW, btnH);
        CameraBtn->OnClicked.AddLambda([this]() { OnCycleCamera(); });
        PlaceButtonTL(*Root, CameraBtn, leftX, y);
        y += CameraBtn->GetSize().y + 8.0f;

        auto section = [&](const std::string& InName, const std::string& InText) {
            auto t = std::make_shared<UTextBlock>(InName);
            t->SetText(InText);
            t->SetFontScale(kFsCaption);
            t->SetColor({0.55f, 0.78f, 1.0f, 1.0f});
            PlaceTextTL(*Root, t, leftX, y);
            y += MeasurePadded(InText, kFsCaption).y + 4.0f;
        };

        section("RLPresetHdr", "PRESETS");
        PresetLowBtn = MakeButton("RLLow", "LOW", kFsCaption, btnW, btnH);
        PresetLowBtn->OnClicked.AddLambda([this]() { ApplyPreset(EGraphicsQuality::Low); });
        PlaceButtonTL(*Root, PresetLowBtn, leftX, y);
        y += PresetLowBtn->GetSize().y + gap;

        PresetMedBtn = MakeButton("RLMed", "MEDIUM", kFsCaption, btnW, btnH);
        PresetMedBtn->OnClicked.AddLambda([this]() { ApplyPreset(EGraphicsQuality::Medium); });
        PlaceButtonTL(*Root, PresetMedBtn, leftX, y);
        y += PresetMedBtn->GetSize().y + gap;

        PresetHighBtn = MakeButton("RLHigh", "HIGH", kFsCaption, btnW, btnH);
        PresetHighBtn->OnClicked.AddLambda([this]() { ApplyPreset(EGraphicsQuality::High); });
        PlaceButtonTL(*Root, PresetHighBtn, leftX, y);
        y += PresetHighBtn->GetSize().y + 10.0f;

        section("RLShadowHdr", "SHADOWS (tap to cycle)");
        ShadowResBtn = MakeButton("RLShadowRes", "Res: 2048", kFsCaption, btnW, btnH);
        ShadowResBtn->OnClicked.AddLambda([this]() { CycleShadowResolution(); });
        PlaceButtonTL(*Root, ShadowResBtn, leftX, y);
        y += ShadowResBtn->GetSize().y + gap;

        CascadeBtn = MakeButton("RLCascades", "Cascades: 4", kFsCaption, btnW, btnH);
        CascadeBtn->OnClicked.AddLambda([this]() { CycleCascadeCount(); });
        PlaceButtonTL(*Root, CascadeBtn, leftX, y);
        y += CascadeBtn->GetSize().y + gap;

        ShadowDistBtn = MakeButton("RLShadowDist", "Distance: 100", kFsCaption, btnW, btnH);
        ShadowDistBtn->OnClicked.AddLambda([this]() { CycleShadowDistance(); });
        PlaceButtonTL(*Root, ShadowDistBtn, leftX, y);
        y += ShadowDistBtn->GetSize().y + gap;

        ShadowFilterBtn = MakeButton("RLShadowFilter", "Filter: PCF3x3", kFsCaption, btnW, btnH);
        ShadowFilterBtn->OnClicked.AddLambda([this]() { CycleShadowFilter(); });
        PlaceButtonTL(*Root, ShadowFilterBtn, leftX, y);
        y += ShadowFilterBtn->GetSize().y + 10.0f;

        section("RLPlanarHdr", "PLANAR");
        PlanarToggleBtn = MakeButton("RLPlanarToggle", "Planar: ON", kFsCaption, btnW, btnH);
        PlanarToggleBtn->OnClicked.AddLambda([this]() { TogglePlanar(); });
        PlaceButtonTL(*Root, PlanarToggleBtn, leftX, y);
        y += PlanarToggleBtn->GetSize().y + gap;

        PlanarQualityBtn = MakeButton("RLPlanarQ", "Quality: Epic", kFsCaption, btnW, btnH);
        PlanarQualityBtn->OnClicked.AddLambda([this]() { CyclePlanarQuality(); });
        PlaceButtonTL(*Root, PlanarQualityBtn, leftX, y);
        y += PlanarQualityBtn->GetSize().y + 10.0f;

        section("RLPostHdr", "POST");
        SsaoBtn = MakeButton("RLSSAO", "SSAO: ON", kFsCaption, btnW, btnH);
        SsaoBtn->OnClicked.AddLambda([this]() { ToggleSSAO(); });
        PlaceButtonTL(*Root, SsaoBtn, leftX, y);
        y += SsaoBtn->GetSize().y + gap;

        BloomBtn = MakeButton("RLBloom", "Bloom: ON", kFsCaption, btnW, btnH);
        BloomBtn->OnClicked.AddLambda([this]() { ToggleBloom(); });
        PlaceButtonTL(*Root, BloomBtn, leftX, y);
        y += BloomBtn->GetSize().y + gap;

        FxaaBtn = MakeButton("RLFXAA", "FXAA: ON", kFsCaption, btnW, btnH);
        FxaaBtn->OnClicked.AddLambda([this]() { ToggleFXAA(); });
        PlaceButtonTL(*Root, FxaaBtn, leftX, y);
        y += FxaaBtn->GetSize().y + 10.0f;

        StatusText = std::make_shared<UTextBlock>("RLStatus");
        StatusText->SetFontScale(kFsCaption);
        StatusText->SetColor({0.75f, 0.82f, 0.92f, 1.0f});
        StatusText->SetText("...");
        PlaceTextTL(*Root, StatusText, leftX, y, btnW, 44.0f);
        y += 46.0f;

        BackBtn = MakeButton("RLBack", "BACK TO MENU", kFsCaption, btnW, btnH);
        BackBtn->OnClicked.AddLambda([this]() { OnBack(); });
        PlaceButtonTL(*Root, BackBtn, leftX, y);

        SetWidgetTree(Root);
        ApplyViewportLayout();
        RefreshLabels();
        RefreshDebugStatus();
    }

    void ULeonTournamentRenderLabWidget::ApplyViewportLayout() {
        if (!Root)
            return;
        SetSize(FLeonTournamentUILayout::ResolveViewportSize(Root.get()));
        FLeonTournamentUILayout::SyncResolutionScale(*Root, AppliedLayoutScale, AppliedViewport);
    }

    void ULeonTournamentRenderLabWidget::Tick(float InDeltaTime) {
        UUserWidget::Tick(InDeltaTime);
        const glm::vec2 vp = FLeonTournamentUILayout::ResolveViewportSize(Root.get());
        const float scale = FUILayout::LayoutScale(vp.x, vp.y);
        if (std::abs(scale - AppliedLayoutScale) > 0.001f || glm::length(vp - AppliedViewport) > 1.0f)
            ApplyViewportLayout();
        if (GamepadEdge(GamepadButton::B, bPadBWasDown))
            OnBack();
        RefreshDebugStatus();
    }

    FGraphicsPreset ULeonTournamentRenderLabWidget::ReadCurrent() const {
        FGraphicsPreset p =
            FGraphicsQuality::GetPreset(EGraphicsQuality::High);
        auto* gi = GI();
        UWorld* world = gi ? gi->GetWorld().get() : nullptr;
        if (!world && OwningPlayer)
            world = OwningPlayer->GetWorld();
        if (!world)
            return p;
        p.ShadowMapResolution = world->GetPendingShadowMapResolution();
        p.CascadeCount = world->GetPendingCascadeCount();
        p.ShadowDistance = world->GetPendingShadowDistance();
        p.ShadowFilter = world->GetPendingShadowFilter();
        p.bEnablePlanarReflection = world->GetPendingPlanarReflectionEnabled();
        p.PlanarQuality = world->GetPendingPlanarReflectionQuality();
        p.bEnableSSAO = world->GetPendingSSAOEnabled();
        p.bEnableBloom = world->GetPendingBloomEnabled();
        p.bEnableFXAA = world->GetPendingFXAAEnabled();
        return p;
    }

    void ULeonTournamentRenderLabWidget::RefreshLabels() {
        const FGraphicsPreset p = ReadCurrent();
        const glm::vec4 selected{0.16f, 0.42f, 0.72f, 1.0f};
        const glm::vec4 idle{0.12f, 0.18f, 0.30f, 0.95f};

        EGraphicsQuality q = EGraphicsQuality::High;
        if (auto* gi = GI()) {
            q = gi->GetGraphicsQuality();
        } else if (OwningPlayer && OwningPlayer->GetWorld()) {
            q = FGraphicsQuality::InferFromWorld(*OwningPlayer->GetWorld());
        }

        if (PresetLowBtn)
            PresetLowBtn->SetNormalColor(q == EGraphicsQuality::Low ? selected : idle);
        if (PresetMedBtn)
            PresetMedBtn->SetNormalColor(q == EGraphicsQuality::Medium ? selected : idle);
        if (PresetHighBtn)
            PresetHighBtn->SetNormalColor(q == EGraphicsQuality::High ? selected : idle);

        char buf[64];
        std::snprintf(buf, sizeof(buf), "Res: %u", p.ShadowMapResolution);
        SetButtonLabel(ShadowResBtn, buf);
        std::snprintf(buf, sizeof(buf), "Cascades: %u", p.CascadeCount);
        SetButtonLabel(CascadeBtn, buf);
        std::snprintf(buf, sizeof(buf), "Distance: %.0f", p.ShadowDistance);
        SetButtonLabel(ShadowDistBtn, buf);
        std::snprintf(buf, sizeof(buf), "Filter: %s", FilterLabel(p.ShadowFilter));
        SetButtonLabel(ShadowFilterBtn, buf);
        SetButtonLabel(PlanarToggleBtn, p.bEnablePlanarReflection ? "Planar: ON" : "Planar: OFF");
        std::snprintf(buf, sizeof(buf), "Quality: %s", PlanarLabel(p.PlanarQuality));
        SetButtonLabel(PlanarQualityBtn, buf);
        SetButtonLabel(SsaoBtn, p.bEnableSSAO ? "SSAO: ON" : "SSAO: OFF");
        SetButtonLabel(BloomBtn, p.bEnableBloom ? "Bloom: ON" : "Bloom: OFF");
        SetButtonLabel(FxaaBtn, p.bEnableFXAA ? "FXAA: ON" : "FXAA: OFF");
        if (auto* gm = dynamic_cast<ALeonTournamentRenderLabGameMode*>(GM(OwningPlayer))) {
            char cam[24];
            std::snprintf(cam, sizeof(cam), "CAM %c", static_cast<char>('A' + gm->GetLabCameraIndex()));
            SetButtonLabel(CameraBtn, cam);
        }
    }

    void ULeonTournamentRenderLabWidget::RefreshDebugStatus() {
        FRenderDebugState state;
        bool bHud = false;
        bool bGizmos = false;
        if (FApplication::HasInstance()) {
            bHud = FApplication::Get().IsHUDEnabled();
            bGizmos = FApplication::Get().IsLightGizmosEnabled();
        }

        UWorld* world = GI() ? GI()->GetWorld().get() : (OwningPlayer ? OwningPlayer->GetWorld() : nullptr);
        FWorldRenderer* renderer = world ? world->GetWorldRenderer() : nullptr;
        if (renderer) {
            state.ShaderMode = renderer->GetDebugMode();
            state.PostProcessDebugMode = renderer->GetPostProcessSettings().DebugMode;
            state.bWireframe = renderer->IsWireframeEnabled();
            state.bPostProcessEnabled = renderer->GetPostProcessSettings().bEnabled;
        }

        if (StatusText) {
            char live[160];
            if (!renderer) {
                StatusText->SetText("LIVE — waiting for renderer");
            } else {
                const auto& sh = renderer->GetShadowSettings();
                const auto& pp = renderer->GetPostProcessSettings();
                std::snprintf(live, sizeof(live), "LIVE %ux CSM%u %s d%.0f  P:%s AO:%s BL:%.2f FX:%s",
                              sh.CascadeResolution, sh.CascadeCount, FilterLabel(sh.FilterMode), sh.ShadowDistance,
                              renderer->IsPlanarReflectionEnabled() ? "ON" : "off", pp.bSSAOEnabled ? "ON" : "off",
                              pp.BloomIntensity, pp.bFXAAEnabled ? "ON" : "off");
                StatusText->SetText(live);
            }
        }

        const char* mat = FRenderDebugHotkeys::CycleNameOrNull(state.ShaderMode, FRenderDebugHotkeys::MaterialCycle);
        const char* geo = FRenderDebugHotkeys::CycleNameOrNull(state.ShaderMode, FRenderDebugHotkeys::GeometryCycle);
        const char* lit = FRenderDebugHotkeys::CycleNameOrNull(state.ShaderMode, FRenderDebugHotkeys::LightingCycle);
        const char* ibl = FRenderDebugHotkeys::CycleNameOrNull(state.ShaderMode, FRenderDebugHotkeys::IBLMapCycle);
        const char* shd = FRenderDebugHotkeys::CycleNameOrNull(state.ShaderMode, FRenderDebugHotkeys::ShadowCycle);
        const bool bPlanar = state.ShaderMode == 13;
        const bool bPostView = state.PostProcessDebugMode != 0;
        const bool bNeedsReset =
            state.ShaderMode != 0 || state.PostProcessDebugMode != 0 || state.bWireframe || !state.bPostProcessEnabled;

        if (DebugViewLine) {
            char view[96];
            if (bPostView)
                std::snprintf(view, sizeof(view), "View: %s", FRenderDebugHotkeys::PostViewName(state.PostProcessDebugMode));
            else
                std::snprintf(view, sizeof(view), "View: %s", FRenderDebugHotkeys::ShaderViewName(state.ShaderMode));
            DebugViewLine->SetText(view);
        }

        const glm::vec4 on{0.40f, 0.95f, 1.0f, 1.0f};
        const glm::vec4 off{0.60f, 0.66f, 0.76f, 1.0f};
        auto setLine = [&](size_t InIndex, bool bActive, const char* InText) {
            if (InIndex >= DebugKeyLines.size() || !DebugKeyLines[InIndex])
                return;
            DebugKeyLines[InIndex]->SetText(InText);
            DebugKeyLines[InIndex]->SetColor(bActive ? on : off);
        };

        auto orOff = [](const char* InName) { return InName ? InName : "off"; };
        char buf[96];
        std::snprintf(buf, sizeof(buf), "F1 HUD %s", bHud ? "ON" : "off");
        setLine(0, bHud, buf);
        std::snprintf(buf, sizeof(buf), "F2 Gizmos %s", bGizmos ? "ON" : "off");
        setLine(1, bGizmos, buf);
        std::snprintf(buf, sizeof(buf), "F3 Wire %s", state.bWireframe ? "ON" : "off");
        setLine(2, state.bWireframe, buf);
        std::snprintf(buf, sizeof(buf), "F4 Mat %s", orOff(mat));
        setLine(3, mat != nullptr, buf);
        std::snprintf(buf, sizeof(buf), "F5 Geo %s", orOff(geo));
        setLine(4, geo != nullptr, buf);
        std::snprintf(buf, sizeof(buf), "F6 Light %s", orOff(lit));
        setLine(5, lit != nullptr, buf);
        std::snprintf(buf, sizeof(buf), "F7 IBL %s", orOff(ibl));
        setLine(6, ibl != nullptr, buf);
        std::snprintf(buf, sizeof(buf), "F8 Shadow %s", orOff(shd));
        setLine(7, shd != nullptr, buf);
        std::snprintf(buf, sizeof(buf), "F9 Planar %s", bPlanar ? "ON" : "off");
        setLine(8, bPlanar, buf);
        const char* postName = FRenderDebugHotkeys::PostViewName(state.PostProcessDebugMode);
        if (std::strncmp(postName, "Post: ", 6) == 0)
            postName += 6;
        std::snprintf(buf, sizeof(buf), "F10 Post %s", bPostView ? postName : "off");
        setLine(9, bPostView, buf);
        std::snprintf(buf, sizeof(buf), "F11 FX %s", state.bPostProcessEnabled ? "ON" : "off");
        setLine(10, state.bPostProcessEnabled, buf);
        std::snprintf(buf, sizeof(buf), "F12 %s", bNeedsReset ? "reset" : "lit");
        setLine(11, bNeedsReset, buf);
    }

    void ULeonTournamentRenderLabWidget::SyncGiQualityFromWorld() {
        UWorld* world = GI() ? GI()->GetWorld().get() : (OwningPlayer ? OwningPlayer->GetWorld() : nullptr);
        if (!world)
            return;
        if (auto* gi = GI())
            gi->SetGraphicsQuality(FGraphicsQuality::InferFromWorld(*world));
    }

    void ULeonTournamentRenderLabWidget::CommitLabKnobs() {
        SyncGiQualityFromWorld();
        if (auto* gm = dynamic_cast<ALeonTournamentRenderLabGameMode*>(GM(OwningPlayer)))
            gm->PunchLabPreview();
        RefreshLabels();
    }

    void ULeonTournamentRenderLabWidget::ApplyPreset(EGraphicsQuality InQuality) {
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_UIClick", 0.5f);
        if (auto* gi = GI()) {
            gi->SetGraphicsQuality(InQuality);
            if (auto world = gi->GetWorld())
                FGraphicsQuality::ApplyToWorld(*world, InQuality);
        } else if (OwningPlayer && OwningPlayer->GetWorld()) {
            FGraphicsQuality::ApplyToWorld(*OwningPlayer->GetWorld(), InQuality);
        }
        if (auto* gm = dynamic_cast<ALeonTournamentRenderLabGameMode*>(GM(OwningPlayer)))
            gm->PunchLabPreview();
        RefreshLabels();
    }

    void ULeonTournamentRenderLabWidget::CycleShadowResolution() {
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_UIClick", 0.45f);
        UWorld* world = GI() ? GI()->GetWorld().get() : (OwningPlayer ? OwningPlayer->GetWorld() : nullptr);
        if (!world)
            return;
        FGraphicsPreset p = ReadCurrent();
        if (p.ShadowMapResolution <= 512)
            p.ShadowMapResolution = 1024;
        else if (p.ShadowMapResolution <= 1024)
            p.ShadowMapResolution = 2048;
        else
            p.ShadowMapResolution = 512;
        world->SetProjectRendererDefaults(p.ShadowMapResolution, p.bEnablePlanarReflection, p.CascadeCount,
                                          p.ShadowDistance, p.PlanarQuality, 0.0f);
        if (UEngine::HasInstance())
            UEngine::Get().SetProjectRendererConfig(p.ShadowMapResolution, p.bEnablePlanarReflection, p.CascadeCount,
                                                    p.ShadowDistance, p.PlanarQuality, p.bEnableSSAO, p.bEnableBloom,
                                                    p.bEnableFXAA, p.ShadowFilter);
        CommitLabKnobs();
    }

    void ULeonTournamentRenderLabWidget::CycleCascadeCount() {
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_UIClick", 0.45f);
        UWorld* world = GI() ? GI()->GetWorld().get() : (OwningPlayer ? OwningPlayer->GetWorld() : nullptr);
        if (!world)
            return;
        FGraphicsPreset p = ReadCurrent();
        p.CascadeCount = p.CascadeCount <= 1 ? 2 : (p.CascadeCount <= 2 ? 4 : 1);
        world->SetProjectRendererDefaults(p.ShadowMapResolution, p.bEnablePlanarReflection, p.CascadeCount,
                                          p.ShadowDistance, p.PlanarQuality, 0.0f);
        if (UEngine::HasInstance())
            UEngine::Get().SetProjectRendererConfig(p.ShadowMapResolution, p.bEnablePlanarReflection, p.CascadeCount,
                                                    p.ShadowDistance, p.PlanarQuality, p.bEnableSSAO, p.bEnableBloom,
                                                    p.bEnableFXAA, p.ShadowFilter);
        CommitLabKnobs();
    }

    void ULeonTournamentRenderLabWidget::CycleShadowDistance() {
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_UIClick", 0.45f);
        UWorld* world = GI() ? GI()->GetWorld().get() : (OwningPlayer ? OwningPlayer->GetWorld() : nullptr);
        if (!world)
            return;
        FGraphicsPreset p = ReadCurrent();
        if (p.ShadowDistance < 12.0f)
            p.ShadowDistance = 16.0f;
        else if (p.ShadowDistance < 28.0f)
            p.ShadowDistance = 40.0f;
        else if (p.ShadowDistance < 60.0f)
            p.ShadowDistance = 80.0f;
        else
            p.ShadowDistance = 8.0f;
        world->SetProjectRendererDefaults(p.ShadowMapResolution, p.bEnablePlanarReflection, p.CascadeCount,
                                          p.ShadowDistance, p.PlanarQuality, 0.0f);
        if (UEngine::HasInstance())
            UEngine::Get().SetProjectRendererConfig(p.ShadowMapResolution, p.bEnablePlanarReflection, p.CascadeCount,
                                                    p.ShadowDistance, p.PlanarQuality, p.bEnableSSAO, p.bEnableBloom,
                                                    p.bEnableFXAA, p.ShadowFilter);
        CommitLabKnobs();
    }

    void ULeonTournamentRenderLabWidget::CycleShadowFilter() {
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_UIClick", 0.45f);
        UWorld* world = GI() ? GI()->GetWorld().get() : (OwningPlayer ? OwningPlayer->GetWorld() : nullptr);
        if (!world)
            return;
        FGraphicsPreset p = ReadCurrent();
        p.ShadowFilter = NextFilter(p.ShadowFilter);
        world->SetProjectShadowFilter(p.ShadowFilter);
        if (UEngine::HasInstance())
            UEngine::Get().SetProjectRendererConfig(p.ShadowMapResolution, p.bEnablePlanarReflection, p.CascadeCount,
                                                    p.ShadowDistance, p.PlanarQuality, p.bEnableSSAO, p.bEnableBloom,
                                                    p.bEnableFXAA, p.ShadowFilter);
        CommitLabKnobs();
    }

    void ULeonTournamentRenderLabWidget::TogglePlanar() {
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_UIClick", 0.45f);
        UWorld* world = GI() ? GI()->GetWorld().get() : (OwningPlayer ? OwningPlayer->GetWorld() : nullptr);
        if (!world)
            return;
        FGraphicsPreset p = ReadCurrent();
        p.bEnablePlanarReflection = !p.bEnablePlanarReflection;
        world->SetProjectRendererDefaults(p.ShadowMapResolution, p.bEnablePlanarReflection, p.CascadeCount,
                                          p.ShadowDistance, p.PlanarQuality, 0.0f);
        if (UEngine::HasInstance())
            UEngine::Get().SetProjectRendererConfig(p.ShadowMapResolution, p.bEnablePlanarReflection, p.CascadeCount,
                                                    p.ShadowDistance, p.PlanarQuality, p.bEnableSSAO, p.bEnableBloom,
                                                    p.bEnableFXAA, p.ShadowFilter);
        CommitLabKnobs();
    }

    void ULeonTournamentRenderLabWidget::CyclePlanarQuality() {
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_UIClick", 0.45f);
        UWorld* world = GI() ? GI()->GetWorld().get() : (OwningPlayer ? OwningPlayer->GetWorld() : nullptr);
        if (!world)
            return;
        FGraphicsPreset p = ReadCurrent();
        p.PlanarQuality = NextPlanar(p.PlanarQuality);
        world->SetProjectRendererDefaults(p.ShadowMapResolution, p.bEnablePlanarReflection, p.CascadeCount,
                                          p.ShadowDistance, p.PlanarQuality, 0.0f);
        if (UEngine::HasInstance())
            UEngine::Get().SetProjectRendererConfig(p.ShadowMapResolution, p.bEnablePlanarReflection, p.CascadeCount,
                                                    p.ShadowDistance, p.PlanarQuality, p.bEnableSSAO, p.bEnableBloom,
                                                    p.bEnableFXAA, p.ShadowFilter);
        CommitLabKnobs();
    }

    void ULeonTournamentRenderLabWidget::ToggleSSAO() {
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_UIClick", 0.45f);
        UWorld* world = GI() ? GI()->GetWorld().get() : (OwningPlayer ? OwningPlayer->GetWorld() : nullptr);
        if (!world)
            return;
        FGraphicsPreset p = ReadCurrent();
        p.bEnableSSAO = !p.bEnableSSAO;
        world->SetProjectSSAODefaults(p.bEnableSSAO, world->GetPendingSSAORadius(), world->GetPendingSSAOIntensity(),
                                      world->GetPendingSSAOBias());
        if (UEngine::HasInstance())
            UEngine::Get().SetProjectRendererConfig(p.ShadowMapResolution, p.bEnablePlanarReflection, p.CascadeCount,
                                                    p.ShadowDistance, p.PlanarQuality, p.bEnableSSAO, p.bEnableBloom,
                                                    p.bEnableFXAA, p.ShadowFilter);
        CommitLabKnobs();
    }

    void ULeonTournamentRenderLabWidget::ToggleBloom() {
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_UIClick", 0.45f);
        UWorld* world = GI() ? GI()->GetWorld().get() : (OwningPlayer ? OwningPlayer->GetWorld() : nullptr);
        if (!world)
            return;
        FGraphicsPreset p = ReadCurrent();
        p.bEnableBloom = !p.bEnableBloom;
        world->SetProjectPostProcessToggles(p.bEnableBloom, p.bEnableFXAA);
        if (UEngine::HasInstance())
            UEngine::Get().SetProjectRendererConfig(p.ShadowMapResolution, p.bEnablePlanarReflection, p.CascadeCount,
                                                    p.ShadowDistance, p.PlanarQuality, p.bEnableSSAO, p.bEnableBloom,
                                                    p.bEnableFXAA, p.ShadowFilter);
        CommitLabKnobs();
    }

    void ULeonTournamentRenderLabWidget::ToggleFXAA() {
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_UIClick", 0.45f);
        UWorld* world = GI() ? GI()->GetWorld().get() : (OwningPlayer ? OwningPlayer->GetWorld() : nullptr);
        if (!world)
            return;
        FGraphicsPreset p = ReadCurrent();
        p.bEnableFXAA = !p.bEnableFXAA;
        world->SetProjectPostProcessToggles(p.bEnableBloom, p.bEnableFXAA);
        if (UEngine::HasInstance())
            UEngine::Get().SetProjectRendererConfig(p.ShadowMapResolution, p.bEnablePlanarReflection, p.CascadeCount,
                                                    p.ShadowDistance, p.PlanarQuality, p.bEnableSSAO, p.bEnableBloom,
                                                    p.bEnableFXAA, p.ShadowFilter);
        CommitLabKnobs();
    }

    void ULeonTournamentRenderLabWidget::OnCycleCamera() {
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_UIClick", 0.45f);
        if (auto* gm = dynamic_cast<ALeonTournamentRenderLabGameMode*>(GM(OwningPlayer)))
            gm->CycleLabCamera();
        RefreshLabels();
    }

    void ULeonTournamentRenderLabWidget::OnBack() {
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_UIClick", 0.5f);
        if (auto* gm = GM(OwningPlayer))
            gm->ReturnToMenu();
    }

} // namespace Leon
