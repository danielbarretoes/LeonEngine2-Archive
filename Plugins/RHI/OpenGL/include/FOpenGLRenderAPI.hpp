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
        void SetClipDistance(bool InEnabled) override;
        void SetPolygonOffset(bool InEnabled, float InFactor = 0.0f, float InUnits = 0.0f) override;

        uint32_t GetFramebufferBinding() override;
        void BindFramebuffer(uint32_t InRendererID) override;

        void DrawArrays(const TRef<FVertexArray>& InVertexArray, unsigned int InVertexCount) override;
        void DrawIndexed(const TRef<FVertexArray>& InVertexArray, unsigned int InIndexCount = 0) override;
        void DrawIndexedOffset(const TRef<FVertexArray>& InVertexArray, unsigned int InIndexCount,
                               unsigned int InIndexOffset) override;
        void DrawIndexedInstanced(const TRef<FVertexArray>& InVertexArray, unsigned int InIndexCount,
                                  unsigned int InInstanceCount) override;
        void DrawIndexedOffsetInstanced(const TRef<FVertexArray>& InVertexArray, unsigned int InIndexCount,
                                        unsigned int InIndexOffset, unsigned int InInstanceCount) override;
        void DrawLines(const TRef<FVertexArray>& InVertexArray, unsigned int InVertexCount) override;
        void SetLineWidth(float InWidth) override;

        FGPUInfo GetGPUInfo() override;
        FGPUVRAMStats GetGPUVRAMStats() override;

        void BeginGPUTimeQuery(uint32_t InSlot) override;
        void EndGPUTimeQuery(uint32_t InSlot) override;
        void ResolveGPUTimeQueries() override;
        float GetGPUTimeMs(uint32_t InSlot) const override;
        void InvalidateShaderBindingCache() override;

        FOpenGLRenderAPI() = default;
        ~FOpenGLRenderAPI() override;

    private:
        void EnsureGPUQueries();

        // CPU State Cache to prevent redundant OpenGL driver state switches
        bool bDepthTestEnabled = false;
        bool bDepthMaskEnabled = true;
        EDepthFunc DepthFunc = EDepthFunc::Less;
        bool bCullEnabled = false;
        ECullMode CullMode = ECullMode::Back;
        bool bBlendEnabled = false;
        EBlendFactor SrcBlend = EBlendFactor::SrcAlpha;
        EBlendFactor DstBlend = EBlendFactor::OneMinusSrcAlpha;
        uint32_t CurrentFBO = 0;
        uint32_t ViewportX = 0, ViewportY = 0, ViewportW = 0, ViewportH = 0;

        static constexpr uint32_t kGPUQuerySlots = 8;
        static constexpr uint32_t kGPUQueryFrames = 3;
        unsigned int GPUQueries[kGPUQueryFrames][kGPUQuerySlots]{};
        bool GPUQueryIssued[kGPUQueryFrames][kGPUQuerySlots]{};
        int GPUWriteFrame = 0;
        int GPUActiveSlot = -1;
        float GPUResolvedMs[kGPUQuerySlots]{};
        bool bGPUQueriesReady = false;
    };

} // namespace Leon
