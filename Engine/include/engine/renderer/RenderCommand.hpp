#pragma once

#include "engine/renderer/RenderAPI.hpp"

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

        static void DrawArrays(const TRef<FVertexArray>& InVertexArray, unsigned int InVertexCount) {
            if (s_RenderAPI)
                s_RenderAPI->DrawArrays(InVertexArray, InVertexCount);
        }

        static void DrawIndexed(const TRef<FVertexArray>& InVertexArray, unsigned int InIndexCount = 0) {
            if (s_RenderAPI)
                s_RenderAPI->DrawIndexed(InVertexArray, InIndexCount);
        }

        static void DrawLines(const TRef<FVertexArray>& InVertexArray, unsigned int InVertexCount) {
            if (s_RenderAPI)
                s_RenderAPI->DrawLines(InVertexArray, InVertexCount);
        }

        static void SetLineWidth(float InWidth) {
            if (s_RenderAPI)
                s_RenderAPI->SetLineWidth(InWidth);
        }

    private:
        static TScope<IRenderAPI> s_RenderAPI;
    };

    using RenderCommand = FRenderCommand;

} // namespace Leon
