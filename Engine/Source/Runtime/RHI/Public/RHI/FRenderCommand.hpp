#pragma once

#include "RHI/IRenderAPI.hpp"

namespace Leon {

    class FRenderCommand {
    public:
        static void Init() {
            if (!RenderAPI) {
                RenderAPI = IRenderAPI::Create();
            }
            if (RenderAPI) {
                RenderAPI->Init();
            }
        }

        static void SetViewport(unsigned int InX, unsigned int InY, unsigned int InWidth, unsigned int InHeight) {
            if (RenderAPI)
                RenderAPI->SetViewport(InX, InY, InWidth, InHeight);
        }

        static void SetClearColor(float InR, float InG, float InB, float InA) {
            if (RenderAPI)
                RenderAPI->SetClearColor(InR, InG, InB, InA);
        }

        static void Clear() {
            if (RenderAPI)
                RenderAPI->Clear();
        }

        static void SetDepthTesting(bool InEnabled) {
            if (RenderAPI)
                RenderAPI->SetDepthTesting(InEnabled);
        }

        static void SetDepthMask(bool InEnabled) {
            if (RenderAPI)
                RenderAPI->SetDepthMask(InEnabled);
        }

        static void SetDepthFunc(EDepthFunc InFunc) {
            if (RenderAPI)
                RenderAPI->SetDepthFunc(InFunc);
        }

        static void SetCulling(bool InEnabled, ECullMode InMode = ECullMode::Back) {
            if (RenderAPI)
                RenderAPI->SetCulling(InEnabled, InMode);
        }

        static void SetWireframe(bool InEnabled) {
            if (RenderAPI)
                RenderAPI->SetWireframe(InEnabled);
        }

        static void SetBlendState(bool InEnabled) {
            if (RenderAPI)
                RenderAPI->SetBlendState(InEnabled);
        }

        static void SetBlendFunc(EBlendFactor InSrc, EBlendFactor InDst) {
            if (RenderAPI)
                RenderAPI->SetBlendFunc(InSrc, InDst);
        }

        static void SetClipDistance(bool InEnabled) {
            if (RenderAPI)
                RenderAPI->SetClipDistance(InEnabled);
        }

        static void SetPolygonOffset(bool InEnabled, float InFactor = 0.0f, float InUnits = 0.0f) {
            if (RenderAPI)
                RenderAPI->SetPolygonOffset(InEnabled, InFactor, InUnits);
        }

        static uint32_t GetFramebufferBinding() { return RenderAPI ? RenderAPI->GetFramebufferBinding() : 0; }

        static void BindFramebuffer(uint32_t InRendererID) {
            if (RenderAPI)
                RenderAPI->BindFramebuffer(InRendererID);
        }

        static void DrawArrays(const TRef<FVertexArray>& InVertexArray, unsigned int InVertexCount);
        static void DrawIndexed(const TRef<FVertexArray>& InVertexArray, unsigned int InIndexCount = 0);
        static void DrawIndexedOffset(const TRef<FVertexArray>& InVertexArray, unsigned int InIndexCount,
                                      unsigned int InIndexOffset);
        static void DrawIndexedInstanced(const TRef<FVertexArray>& InVertexArray, unsigned int InIndexCount,
                                         unsigned int InInstanceCount);
        static void DrawIndexedOffsetInstanced(const TRef<FVertexArray>& InVertexArray, unsigned int InIndexCount,
                                               unsigned int InIndexOffset, unsigned int InInstanceCount);
        static void DrawLines(const TRef<FVertexArray>& InVertexArray, unsigned int InVertexCount);

        static void SetLineWidth(float InWidth) {
            if (RenderAPI)
                RenderAPI->SetLineWidth(InWidth);
        }

        static FGPUInfo GetGPUInfo() { return RenderAPI ? RenderAPI->GetGPUInfo() : FGPUInfo{}; }

        static FGPUVRAMStats GetGPUVRAMStats() {
            return RenderAPI ? RenderAPI->GetGPUVRAMStats() : FGPUVRAMStats{};
        }

        static void BeginGPUTimeQuery(uint32_t InSlot) {
            if (RenderAPI)
                RenderAPI->BeginGPUTimeQuery(InSlot);
        }
        static void EndGPUTimeQuery(uint32_t InSlot) {
            if (RenderAPI)
                RenderAPI->EndGPUTimeQuery(InSlot);
        }
        static void ResolveGPUTimeQueries() {
            if (RenderAPI)
                RenderAPI->ResolveGPUTimeQueries();
        }
        static float GetGPUTimeMs(uint32_t InSlot) {
            return RenderAPI ? RenderAPI->GetGPUTimeMs(InSlot) : 0.0f;
        }

        /** Release the process IRenderAPI while the graphics context is still alive. */
        static void Shutdown() { RenderAPI.reset(); }

    private:
        static TScope<IRenderAPI> RenderAPI;
    };

} // namespace Leon
