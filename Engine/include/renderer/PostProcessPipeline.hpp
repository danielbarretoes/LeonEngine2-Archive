#pragma once

#include "core/Base.hpp"
#include "renderer/Framebuffer.hpp"
#include "renderer/Shader.hpp"
#include "renderer/VertexArray.hpp"
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

        void Render(const FPostProcessSettings& InSettings,
                    TRef<FFramebuffer> InHDRScene,
                    uint32_t InTargetFBO,
                    uint32_t InVpWidth,
                    uint32_t InVpHeight);

        // Accessors for testing and debugging
        TRef<FFramebuffer> GetToneMappedFBO() const { return m_ToneMappedFBO; }
        const std::vector<TRef<FFramebuffer>>& GetBloomDownsampleFBOs() const { return m_BloomDownsampleFBOs; }
        const std::vector<TRef<FFramebuffer>>& GetBloomUpsampleFBOs() const { return m_BloomUpsampleFBOs; }

        TRef<FShader> GetBloomBrightPassShader() const { return m_BloomBrightPassShader; }
        TRef<FShader> GetBloomDownsampleShader() const { return m_BloomDownsampleShader; }
        TRef<FShader> GetBloomUpsampleShader() const { return m_BloomUpsampleShader; }
        TRef<FShader> GetToneMappingShader() const { return m_ToneMappingShader; }
        TRef<FShader> GetFXAAShader() const { return m_FXAAShader; }

    private:
        void InvalidateFramebuffers(uint32_t InWidth, uint32_t InHeight);
        void RenderBloom(const FPostProcessSettings& InSettings, TRef<FFramebuffer> InHDRScene, uint32_t InWidth, uint32_t InHeight);

    private:
        uint32_t m_Width = 1280;
        uint32_t m_Height = 720;
        bool m_bInitialized = false;

        // Shaders
        TRef<FShader> m_BloomBrightPassShader;
        TRef<FShader> m_BloomDownsampleShader;
        TRef<FShader> m_BloomUpsampleShader;
        TRef<FShader> m_ToneMappingShader;
        TRef<FShader> m_FXAAShader;

        // Geometry
        TRef<FVertexArray> m_FullscreenQuadVA;

        // Framebuffers
        std::vector<TRef<FFramebuffer>> m_BloomDownsampleFBOs;
        std::vector<TRef<FFramebuffer>> m_BloomUpsampleFBOs;
        TRef<FFramebuffer> m_ToneMappedFBO;
    };

} // namespace Leon
