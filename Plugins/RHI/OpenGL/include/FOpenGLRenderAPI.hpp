#pragma once

#include "RHI/IRenderAPI.hpp"

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
        bool DepthTestEnabled = false;
        bool DepthMaskEnabled = true;
        EDepthFunc DepthFunc = EDepthFunc::Less;
        bool CullEnabled = false;
        ECullMode CullMode = ECullMode::Back;
        bool BlendEnabled = false;
        EBlendFactor SrcBlend = EBlendFactor::SrcAlpha;
        EBlendFactor DstBlend = EBlendFactor::OneMinusSrcAlpha;
        uint32_t CurrentFBO = 0;
        uint32_t ViewportX = 0, ViewportY = 0, ViewportW = 0, ViewportH = 0;
    };

} // namespace Leon
