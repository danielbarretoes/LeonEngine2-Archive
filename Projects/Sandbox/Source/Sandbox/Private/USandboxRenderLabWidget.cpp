#include "USandboxRenderLabWidget.hpp"
#include "UMG/FUILayout.hpp"
#include "UMG/FUIRenderer.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Engine/UEngine.hpp"
#include "Engine/UWorld.hpp"
#include "Core/FApplication.hpp"
#include "Renderer/FShadowTypes.hpp"
#include "Renderer/FPlanarReflectionTypes.hpp"
#include "Renderer/FWorldRenderer.hpp"
#include "Renderer/FRenderDebugHotkeys.hpp"
#include "Engine/FGraphicsQuality.hpp"
#include "RHI/FRenderer.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace Leon {

    namespace {
        constexpr float kFsCaption = FUILayout::kFsCaption;

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

    USandboxRenderLabWidget::USandboxRenderLabWidget(const std::string& InName) : UUserWidget(InName) {}

    void USandboxRenderLabWidget::Construct() {
        Build();
    }

    void USandboxRenderLabWidget::SetLabVisible(bool bVisible) {
        bLabVisible = bVisible;
        SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        if (!OwningPlayer)
            return;
        if (bVisible) {
            OwningPlayer->SetInputModeUIOnly();
            OwningPlayer->SetShowMouseCursor(true);
        } else {
            OwningPlayer->SetInputModeGameOnly();
            OwningPlayer->SetShowMouseCursor(false);
        }
    }

    void USandboxRenderLabWidget::Build() {
        FUIRenderer::Init();
        Root = std::make_shared<UCanvasPanel>("SandboxRenderLabRoot");
        Root->SetSize({1280, 720});
        Root->SetBackgroundColor({0.0f, 0.0f, 0.0f, 0.0f});

        constexpr float panelW = 232.0f;
        constexpr float panelH = 520.0f;
        constexpr float designW = 1280.0f;
        Panel = std::make_shared<UImage>("SandboxRLPanel");
        Panel->SetTintColor({0.02f, 0.03f, 0.06f, 0.82f});
        Root->AddChild(Panel, FAnchors::TopLeft(), FUILayout::BoxTL(designW - panelW - 8.0f, 8.0f, panelW, panelH));

        constexpr float debugW = 248.0f;
        constexpr float debugH = 214.0f;
        DebugPanel = std::make_shared<UImage>("SandboxRLDebugPanel");
        DebugPanel->SetTintColor({0.02f, 0.03f, 0.06f, 0.80f});
        Root->AddChild(DebugPanel, FAnchors::TopLeft(), FUILayout::BoxTL(8.0f, 8.0f, debugW, debugH));

        constexpr float debugX = 16.0f;
        float debugY = 14.0f;
        DebugTitle = std::make_shared<UTextBlock>("SBRLDebugTitle");
        DebugTitle->SetText("DEBUG");
        DebugTitle->SetFontScale(kFsCaption);
        DebugTitle->SetColor({0.92f, 0.95f, 1.0f, 1.0f});
        FUILayout::PlaceTextTL(*Root, DebugTitle, debugX, debugY);
        debugY += FUILayout::MeasurePadded(DebugTitle->GetText(), DebugTitle->GetFontScale()).y;

        DebugViewLine = std::make_shared<UTextBlock>("SBRLDebugView");
        DebugViewLine->SetFontScale(kFsCaption);
        DebugViewLine->SetColor({0.45f, 0.92f, 1.0f, 1.0f});
        DebugViewLine->SetText("View: Lit");
        FUILayout::PlaceTextTL(*Root, DebugViewLine, debugX, debugY, 220.0f, 14.0f);
        debugY += 14.0f;

        const char* keyPlaceholders[12] = {
            "F1 HUD off",  "F2 Gizmos off", "F3 Wire off", "F4 Mat off", "F5 Geo off",  "F6 Light off",
            "F7 IBL off",  "F8 Shadow off", "F9 Planar off", "F10 Post off", "F11 FX on", "F12 Reset",
        };
        for (int i = 0; i < 12; ++i) {
            char name[24];
            std::snprintf(name, sizeof(name), "SBRLDebugF%d", i + 1);
            DebugKeyLines[static_cast<size_t>(i)] = std::make_shared<UTextBlock>(name);
            DebugKeyLines[static_cast<size_t>(i)]->SetFontScale(kFsCaption);
            DebugKeyLines[static_cast<size_t>(i)]->SetColor({0.62f, 0.68f, 0.78f, 1.0f});
            DebugKeyLines[static_cast<size_t>(i)]->SetText(keyPlaceholders[i]);
            FUILayout::PlaceTextTL(*Root, DebugKeyLines[static_cast<size_t>(i)], debugX, debugY, 220.0f, 13.0f);
            debugY += 13.0f;
        }

        constexpr float leftX = designW - panelW + 4.0f;
        float y = 16.0f;
        constexpr float gap = 3.0f;
        constexpr float btnW = 208.0f;
        constexpr float btnH = 24.0f;

        auto title = std::make_shared<UTextBlock>("SBRLTitle");
        title->SetText("RENDER LAB");
        title->SetFontScale(kFsCaption);
        title->SetColor({0.92f, 0.95f, 1.0f, 1.0f});
        FUILayout::PlaceTextTL(*Root, title, leftX, y);
        y += FUILayout::MeasurePadded(title->GetText(), title->GetFontScale()).y + 2.0f;

        auto hint = std::make_shared<UTextBlock>("SBRLHint");
        hint->SetText("F-keys left. H=hide");
        hint->SetFontScale(kFsCaption);
        hint->SetColor({0.65f, 0.72f, 0.85f, 1.0f});
        FUILayout::PlaceTextTL(*Root, hint, leftX, y);
        y += FUILayout::MeasurePadded(hint->GetText(), hint->GetFontScale()).y + 8.0f;

        auto section = [&](const std::string& InName, const std::string& InText) {
            auto t = std::make_shared<UTextBlock>(InName);
            t->SetText(InText);
            t->SetFontScale(kFsCaption);
            t->SetColor({0.55f, 0.78f, 1.0f, 1.0f});
            FUILayout::PlaceTextTL(*Root, t, leftX, y);
            y += FUILayout::MeasurePadded(InText, kFsCaption).y + 4.0f;
        };

        section("SBRLPresetHdr", "PRESETS");
        PresetLowBtn = FUILayout::MakeButton("SBRLLow", "LOW", kFsCaption, btnW, btnH);
        PresetLowBtn->OnClicked.AddLambda([this]() { ApplyPreset(EGraphicsQuality::Low); });
        FUILayout::PlaceButtonTL(*Root, PresetLowBtn, leftX, y);
        y += PresetLowBtn->GetSize().y + gap;

        PresetMedBtn = FUILayout::MakeButton("SBRLMed", "MEDIUM", kFsCaption, btnW, btnH);
        PresetMedBtn->OnClicked.AddLambda([this]() { ApplyPreset(EGraphicsQuality::Medium); });
        FUILayout::PlaceButtonTL(*Root, PresetMedBtn, leftX, y);
        y += PresetMedBtn->GetSize().y + gap;

        PresetHighBtn = FUILayout::MakeButton("SBRLHigh", "HIGH", kFsCaption, btnW, btnH);
        PresetHighBtn->OnClicked.AddLambda([this]() { ApplyPreset(EGraphicsQuality::High); });
        FUILayout::PlaceButtonTL(*Root, PresetHighBtn, leftX, y);
        y += PresetHighBtn->GetSize().y + 10.0f;

        section("SBRLShadowHdr", "SHADOWS (tap to cycle)");
        ShadowResBtn = FUILayout::MakeButton("SBRLShadowRes", "CSM: 2048", kFsCaption, btnW, btnH);
        ShadowResBtn->OnClicked.AddLambda([this]() { CycleShadowResolution(); });
        FUILayout::PlaceButtonTL(*Root, ShadowResBtn, leftX, y);
        y += ShadowResBtn->GetSize().y + gap;

        CascadeBtn = FUILayout::MakeButton("SBRLCascades", "Cascades: 4", kFsCaption, btnW, btnH);
        CascadeBtn->OnClicked.AddLambda([this]() { CycleCascadeCount(); });
        FUILayout::PlaceButtonTL(*Root, CascadeBtn, leftX, y);
        y += CascadeBtn->GetSize().y + gap;

        ShadowDistBtn = FUILayout::MakeButton("SBRLShadowDist", "Distance: 100", kFsCaption, btnW, btnH);
        ShadowDistBtn->OnClicked.AddLambda([this]() { CycleShadowDistance(); });
        FUILayout::PlaceButtonTL(*Root, ShadowDistBtn, leftX, y);
        y += ShadowDistBtn->GetSize().y + gap;

        ShadowFilterBtn = FUILayout::MakeButton("SBRLShadowFilter", "Filter: PCF3x3", kFsCaption, btnW, btnH);
        ShadowFilterBtn->OnClicked.AddLambda([this]() { CycleShadowFilter(); });
        FUILayout::PlaceButtonTL(*Root, ShadowFilterBtn, leftX, y);
        y += ShadowFilterBtn->GetSize().y + 10.0f;

        section("SBRLPlanarHdr", "PLANAR");
        PlanarToggleBtn = FUILayout::MakeButton("SBRLPlanarToggle", "Planar: ON", kFsCaption, btnW, btnH);
        PlanarToggleBtn->OnClicked.AddLambda([this]() { TogglePlanar(); });
        FUILayout::PlaceButtonTL(*Root, PlanarToggleBtn, leftX, y);
        y += PlanarToggleBtn->GetSize().y + gap;

        PlanarQualityBtn = FUILayout::MakeButton("SBRLPlanarQ", "Quality: Epic", kFsCaption, btnW, btnH);
        PlanarQualityBtn->OnClicked.AddLambda([this]() { CyclePlanarQuality(); });
        FUILayout::PlaceButtonTL(*Root, PlanarQualityBtn, leftX, y);
        y += PlanarQualityBtn->GetSize().y + 10.0f;

        section("SBRLPostHdr", "POST");
        SsaoBtn = FUILayout::MakeButton("SBRLSSAO", "SSAO: ON", kFsCaption, btnW, btnH);
        SsaoBtn->OnClicked.AddLambda([this]() { ToggleSSAO(); });
        FUILayout::PlaceButtonTL(*Root, SsaoBtn, leftX, y);
        y += SsaoBtn->GetSize().y + gap;

        BloomBtn = FUILayout::MakeButton("SBRLBloom", "Bloom: ON", kFsCaption, btnW, btnH);
        BloomBtn->OnClicked.AddLambda([this]() { ToggleBloom(); });
        FUILayout::PlaceButtonTL(*Root, BloomBtn, leftX, y);
        y += BloomBtn->GetSize().y + gap;

        FxaaBtn = FUILayout::MakeButton("SBRLFXAA", "FXAA: ON", kFsCaption, btnW, btnH);
        FxaaBtn->OnClicked.AddLambda([this]() { ToggleFXAA(); });
        FUILayout::PlaceButtonTL(*Root, FxaaBtn, leftX, y);
        y += FxaaBtn->GetSize().y + 10.0f;

        StatusText = std::make_shared<UTextBlock>("SBRLStatus");
        StatusText->SetFontScale(kFsCaption);
        StatusText->SetColor({0.75f, 0.82f, 0.92f, 1.0f});
        StatusText->SetText("...");
        FUILayout::PlaceTextTL(*Root, StatusText, leftX, y, btnW, 44.0f);
        y += 46.0f;

        HideBtn = FUILayout::MakeButton("SBRLHide", "HIDE (H)", kFsCaption, btnW, btnH);
        HideBtn->OnClicked.AddLambda([this]() { SetLabVisible(false); });
        FUILayout::PlaceButtonTL(*Root, HideBtn, leftX, y);

        SetWidgetTree(Root);
        ApplyViewportLayout();
        RefreshLabels();
        RefreshDebugStatus();
    }

    void USandboxRenderLabWidget::ApplyViewportLayout() {
        if (!Root)
            return;
        SetSize(FUILayout::ResolveViewportSize(Root.get()));
        FUILayout::SyncResolutionScale(*Root, AppliedLayoutScale, AppliedViewport);
    }

    void USandboxRenderLabWidget::Tick(float InDeltaTime) {
        UUserWidget::Tick(InDeltaTime);
        if (!bLabVisible)
            return;
        const glm::vec2 vp = FUILayout::ResolveViewportSize(Root.get());
        const float scale = FUILayout::LayoutScale(vp.x, vp.y);
        if (std::abs(scale - AppliedLayoutScale) > 0.001f || glm::length(vp - AppliedViewport) > 1.0f)
            ApplyViewportLayout();
        RefreshDebugStatus();
    }

    UWorld* USandboxRenderLabWidget::ResolveWorld() const {
        return OwningPlayer ? OwningPlayer->GetWorld() : nullptr;
    }

    FGraphicsPreset USandboxRenderLabWidget::ReadCurrent() const {
        FGraphicsPreset p = FGraphicsQuality::GetPreset(SelectedQuality);
        UWorld* world = ResolveWorld();
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

    void USandboxRenderLabWidget::RefreshLabels() {
        const FGraphicsPreset p = ReadCurrent();
        const glm::vec4 selected{0.16f, 0.42f, 0.72f, 1.0f};
        const glm::vec4 idle{0.12f, 0.18f, 0.30f, 0.95f};

        if (UWorld* world = ResolveWorld())
            SelectedQuality = FGraphicsQuality::InferFromWorld(*world);

        if (PresetLowBtn)
            PresetLowBtn->SetNormalColor(SelectedQuality == EGraphicsQuality::Low ? selected : idle);
        if (PresetMedBtn)
            PresetMedBtn->SetNormalColor(SelectedQuality == EGraphicsQuality::Medium ? selected : idle);
        if (PresetHighBtn)
            PresetHighBtn->SetNormalColor(SelectedQuality == EGraphicsQuality::High ? selected : idle);

        SetButtonLabel(PresetLowBtn, FGraphicsQuality::FormatPresetLabel(EGraphicsQuality::Low));
        SetButtonLabel(PresetMedBtn, FGraphicsQuality::FormatPresetLabel(EGraphicsQuality::Medium));
        SetButtonLabel(PresetHighBtn, FGraphicsQuality::FormatPresetLabel(EGraphicsQuality::High));

        char buf[64];
        std::snprintf(buf, sizeof(buf), "CSM: %u", p.ShadowMapResolution);
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
    }

    void USandboxRenderLabWidget::RefreshDebugStatus() {
        FRenderDebugState state;
        bool bHud = false;
        bool bGizmos = false;
        if (FApplication::HasInstance()) {
            bHud = FApplication::Get().IsHUDEnabled();
            bGizmos = FApplication::Get().IsLightGizmosEnabled();
        }

        UWorld* world = ResolveWorld();
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
                std::snprintf(live, sizeof(live), "LIVE %ux CSM%u %s d%.0f  Tex:%u  P:%s AO:%s BL:%.2f FX:%s",
                              sh.CascadeResolution, sh.CascadeCount, FilterLabel(sh.FilterMode), sh.ShadowDistance,
                              FRenderer::GetMaxTextureResolution(),
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
                std::snprintf(view, sizeof(view), "View: %s",
                              FRenderDebugHotkeys::PostViewName(state.PostProcessDebugMode));
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

    void USandboxRenderLabWidget::PunchLabPreview() {
        UWorld* world = ResolveWorld();
        if (!world)
            return;
        FWorldRenderer* renderer = world->GetWorldRenderer();
        if (!renderer)
            return;
        auto& pp = renderer->GetPostProcessSettings();
        if (pp.bBloomEnabled) {
            pp.BloomIntensity = 0.7f;
            pp.BloomThreshold = 0.35f;
        } else {
            pp.BloomIntensity = 0.0f;
            pp.BloomThreshold = 1.2f;
        }
        if (pp.bSSAOEnabled) {
            pp.SSAORadius = 1.5f;
            pp.SSAOIntensity = 2.8f;
            pp.SSAOBias = 0.018f;
        }
    }

    void USandboxRenderLabWidget::PersistCurrent() {
        FGraphicsQuality::PersistToEngineIni(SelectedQuality);
    }

    void USandboxRenderLabWidget::CommitLabKnobs() {
        if (UWorld* world = ResolveWorld())
            SelectedQuality = FGraphicsQuality::InferFromWorld(*world);
        PunchLabPreview();
        RefreshLabels();
    }

    void USandboxRenderLabWidget::ApplyPreset(EGraphicsQuality InQuality) {
        SelectedQuality = InQuality;
        if (UWorld* world = ResolveWorld())
            FGraphicsQuality::ApplyToWorld(*world, InQuality);
        PunchLabPreview();
        PersistCurrent();
        RefreshLabels();
    }

    void USandboxRenderLabWidget::CycleShadowResolution() {
        UWorld* world = ResolveWorld();
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

    void USandboxRenderLabWidget::CycleCascadeCount() {
        UWorld* world = ResolveWorld();
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

    void USandboxRenderLabWidget::CycleShadowDistance() {
        UWorld* world = ResolveWorld();
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

    void USandboxRenderLabWidget::CycleShadowFilter() {
        UWorld* world = ResolveWorld();
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

    void USandboxRenderLabWidget::TogglePlanar() {
        UWorld* world = ResolveWorld();
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

    void USandboxRenderLabWidget::CyclePlanarQuality() {
        UWorld* world = ResolveWorld();
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

    void USandboxRenderLabWidget::ToggleSSAO() {
        UWorld* world = ResolveWorld();
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

    void USandboxRenderLabWidget::ToggleBloom() {
        UWorld* world = ResolveWorld();
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

    void USandboxRenderLabWidget::ToggleFXAA() {
        UWorld* world = ResolveWorld();
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

} // namespace Leon
