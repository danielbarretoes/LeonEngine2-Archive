#pragma once

#include "Core/Base.hpp"
#include "RHI/FFramebuffer.hpp"
#include "RHI/FShader.hpp"
#include "RHI/FVertexArray.hpp"
#include <vector>

namespace Leon {

    struct FPostProcessSettings {
        bool bEnabled = true;

        // Bloom Configuration
        bool bBloomEnabled = true;
        float BloomThreshold = 1.0f;
        float BloomSoftKnee = 0.5f;
        float BloomIntensity = 0.05f;
        float BloomRadius = 1.0f;
        uint32_t BloomMipCount = 5;

        // Tone Mapping Configuration
        int ToneMapper = 0; // 0 = ACES Filmic, 1 = Reinhard Extended, 2 = Neutral Clamp, 3 = Uncharted 2
        float Exposure = 1.0f;
        float Gamma = 2.2f;

        // Anti-Aliasing (FXAA)
        bool bFXAAEnabled = true;

        // Forensic Debug Mode
        // 0: Full Composite Post-Process
        // 1: Raw HDR Linear Scene (Before Tone Mapping)
        // 2: Bloom Glow Output Only
        // 3: Bright Pass Extraction Only
        // 4: Tone Mapping Output Only (No FXAA)
        int DebugMode = 0;
    };

    class FPostProcessPipeline {
    public:
        FPostProcessPipeline();
        ~FPostProcessPipeline() = default;

        void Init();
        void OnViewportResize(uint32_t InWidth, uint32_t InHeight);

        void Render(const FPostProcessSettings& InSettings, TRef<FFramebuffer> InHDRScene, uint32_t InTargetFBO,
                    uint32_t InVpWidth, uint32_t InVpHeight);

        // Accessors for testing and debugging
        TRef<FFramebuffer> GetToneMappedFBO() const { return ToneMappedFBO; }
        const std::vector<TRef<FFramebuffer>>& GetBloomDownsampleFBOs() const { return BloomDownsampleFBOs; }
        const std::vector<TRef<FFramebuffer>>& GetBloomUpsampleFBOs() const { return BloomUpsampleFBOs; }

        TRef<FShader> GetBloomBrightPassShader() const { return BloomBrightPassShader; }
        TRef<FShader> GetBloomDownsampleShader() const { return BloomDownsampleShader; }
        TRef<FShader> GetBloomUpsampleShader() const { return BloomUpsampleShader; }
        TRef<FShader> GetToneMappingShader() const { return ToneMappingShader; }
        TRef<FShader> GetFXAAShader() const { return FXAAShader; }

    private:
        void InvalidateFramebuffers(uint32_t InWidth, uint32_t InHeight);
        void RenderBloom(const FPostProcessSettings& InSettings, TRef<FFramebuffer> InHDRScene, uint32_t InWidth,
                         uint32_t InHeight);

    private:
        uint32_t Width = 1280;
        uint32_t Height = 720;
        bool bInitialized = false;

        // Shaders
        TRef<FShader> BloomBrightPassShader;
        TRef<FShader> BloomDownsampleShader;
        TRef<FShader> BloomUpsampleShader;
        TRef<FShader> ToneMappingShader;
        TRef<FShader> FXAAShader;

        // Geometry
        TRef<FVertexArray> FullscreenQuadVA;

        // Framebuffers
        std::vector<TRef<FFramebuffer>> BloomDownsampleFBOs;
        std::vector<TRef<FFramebuffer>> BloomUpsampleFBOs;
        TRef<FFramebuffer> ToneMappedFBO;
    };

} // namespace Leon
