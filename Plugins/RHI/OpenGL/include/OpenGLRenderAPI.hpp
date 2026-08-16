#pragma once

#include "renderer/RenderAPI.hpp"

namespace Leon {

    class FOpenGLRenderAPI : public IRenderAPI {
    public:
        void Init() override;
        void SetViewport(unsigned int InX, unsigned int InY, unsigned int InWidth, unsigned int InHeight) override;
        void SetClearColor(float InR, float InG, float InB, float InA) override;
        void Clear() override;

        void SetDepthTesting(bool InEnabled) override;
        void SetDepthMask(bool InEnabled) override;
        void SetDepthFunc(EDepthFunc InFunc) override;
        void SetCulling(bool InEnabled, ECullMode InMode = ECullMode::Back) override;
        void SetWireframe(bool InEnabled) override;
        void SetBlendState(bool InEnabled) override;
        void SetBlendFunc(EBlendFactor InSrc, EBlendFactor InDst) override;

        uint32_t GetFramebufferBinding() override;
        void BindFramebuffer(uint32_t InRendererID) override;

        void DrawArrays(const TRef<FVertexArray>& InVertexArray, unsigned int InVertexCount) override;
        void DrawIndexed(const TRef<FVertexArray>& InVertexArray, unsigned int InIndexCount = 0) override;
        void DrawIndexedOffset(const TRef<FVertexArray>& InVertexArray, unsigned int InIndexCount,
                               unsigned int InIndexOffset) override;
        void DrawLines(const TRef<FVertexArray>& InVertexArray, unsigned int InVertexCount) override;
        void SetLineWidth(float InWidth) override;

        FGPUInfo GetGPUInfo() override;
        FGPUVRAMStats GetGPUVRAMStats() override;

    private:
        // CPU State Cache to prevent redundant OpenGL driver state switches
        bool m_DepthTestEnabled = false;
        bool m_DepthMaskEnabled = true;
        EDepthFunc m_DepthFunc = EDepthFunc::Less;
        bool m_CullEnabled = false;
        ECullMode m_CullMode = ECullMode::Back;
        bool m_BlendEnabled = false;
        EBlendFactor m_SrcBlend = EBlendFactor::SrcAlpha;
        EBlendFactor m_DstBlend = EBlendFactor::OneMinusSrcAlpha;
        uint32_t m_CurrentFBO = 0;
        uint32_t m_ViewportX = 0, m_ViewportY = 0, m_ViewportW = 0, m_ViewportH = 0;
    };

    using OpenGLRenderAPI = FOpenGLRenderAPI;

} // namespace Leon
