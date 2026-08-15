#pragma once

#include "renderer/RenderAPI.hpp"

namespace Leon {

    class FRenderCommand {
    public:
        static void Init() {
            if (!s_RenderAPI) {
                s_RenderAPI = IRenderAPI::Create();
            }
            if (s_RenderAPI) {
                s_RenderAPI->Init();
            }
        }

        static void SetViewport(unsigned int InX, unsigned int InY, unsigned int InWidth, unsigned int InHeight) {
            if (s_RenderAPI)
                s_RenderAPI->SetViewport(InX, InY, InWidth, InHeight);
        }

        static void SetClearColor(float InR, float InG, float InB, float InA) {
            if (s_RenderAPI)
                s_RenderAPI->SetClearColor(InR, InG, InB, InA);
        }

        static void Clear() {
            if (s_RenderAPI)
                s_RenderAPI->Clear();
        }

        static void SetDepthTesting(bool InEnabled) {
            if (s_RenderAPI)
                s_RenderAPI->SetDepthTesting(InEnabled);
        }

        static void SetDepthMask(bool InEnabled) {
            if (s_RenderAPI)
                s_RenderAPI->SetDepthMask(InEnabled);
        }

        static void SetDepthFunc(EDepthFunc InFunc) {
            if (s_RenderAPI)
                s_RenderAPI->SetDepthFunc(InFunc);
        }

        static void SetCulling(bool InEnabled, ECullMode InMode = ECullMode::Back) {
            if (s_RenderAPI)
                s_RenderAPI->SetCulling(InEnabled, InMode);
        }

        static void SetBlendState(bool InEnabled) {
            if (s_RenderAPI)
                s_RenderAPI->SetBlendState(InEnabled);
        }

        static void SetBlendFunc(EBlendFactor InSrc, EBlendFactor InDst) {
            if (s_RenderAPI)
                s_RenderAPI->SetBlendFunc(InSrc, InDst);
        }

        static uint32_t GetFramebufferBinding() { return s_RenderAPI ? s_RenderAPI->GetFramebufferBinding() : 0; }

        static void BindFramebuffer(uint32_t InRendererID) {
            if (s_RenderAPI)
                s_RenderAPI->BindFramebuffer(InRendererID);
        }

        static void DrawArrays(const TRef<FVertexArray>& InVertexArray, unsigned int InVertexCount);
        static void DrawIndexed(const TRef<FVertexArray>& InVertexArray, unsigned int InIndexCount = 0);
        static void DrawIndexedOffset(const TRef<FVertexArray>& InVertexArray, unsigned int InIndexCount,
                                      unsigned int InIndexOffset);
        static void DrawLines(const TRef<FVertexArray>& InVertexArray, unsigned int InVertexCount);

        static void SetLineWidth(float InWidth) {
            if (s_RenderAPI)
                s_RenderAPI->SetLineWidth(InWidth);
        }

        static FGPUInfo GetGPUInfo() { return s_RenderAPI ? s_RenderAPI->GetGPUInfo() : FGPUInfo{}; }

        static FGPUVRAMStats GetGPUVRAMStats() {
            return s_RenderAPI ? s_RenderAPI->GetGPUVRAMStats() : FGPUVRAMStats{};
        }

    private:
        static TScope<IRenderAPI> s_RenderAPI;
    };

} // namespace Leon
