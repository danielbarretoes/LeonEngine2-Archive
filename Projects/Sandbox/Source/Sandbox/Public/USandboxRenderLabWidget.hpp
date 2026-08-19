#pragma once

#include "UMG/UUserWidget.hpp"
#include "UMG/UButton.hpp"
#include "UMG/UTextBlock.hpp"
#include "UMG/UCanvasPanel.hpp"
#include "UMG/UImage.hpp"
#include "Engine/FGraphicsQuality.hpp"

#include <array>
#include <cstdint>

namespace Leon {

    /**
     * Graphics A/B overlay for Sandbox Showcase: presets + per-category knobs + F-key status.
     * Toggle visibility from ASandboxHUD (H).
     */
    class USandboxRenderLabWidget : public UUserWidget {
    public:
        USandboxRenderLabWidget(const std::string& InName = "SandboxRenderLab");
        void Construct() override;
        void Tick(float InDeltaTime) override;

        void SetLabVisible(bool bVisible);
        bool IsLabVisible() const { return bLabVisible; }

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
        void PunchLabPreview();
        void CommitLabKnobs();
        void PersistCurrent();
        UWorld* ResolveWorld() const;
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
        TRef<UButton> HideBtn;
        float AppliedLayoutScale = 0.0f;
        glm::vec2 AppliedViewport{0.0f, 0.0f};
        EGraphicsQuality SelectedQuality = EGraphicsQuality::High;
        bool bLabVisible = true;
    };

} // namespace Leon
