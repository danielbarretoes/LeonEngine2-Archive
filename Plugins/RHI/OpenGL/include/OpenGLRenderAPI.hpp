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
        void SetBlendState(bool InEnabled) override;

        uint32_t GetFramebufferBinding() override;
        void BindFramebuffer(uint32_t InRendererID) override;

        void DrawArrays(const TRef<FVertexArray>& InVertexArray, unsigned int InVertexCount) override;
        void DrawIndexed(const TRef<FVertexArray>& InVertexArray, unsigned int InIndexCount = 0) override;
        void DrawLines(const TRef<FVertexArray>& InVertexArray, unsigned int InVertexCount) override;
        void SetLineWidth(float InWidth) override;
    };

    using OpenGLRenderAPI = FOpenGLRenderAPI;

} // namespace Leon
