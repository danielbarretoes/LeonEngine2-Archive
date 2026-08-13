#include "engine/renderer/Renderer.hpp"
#include "engine/core/Log.hpp"

namespace Leon {

    void FRenderer::Init() {
        LE_CORE_INFO("Initializing Renderer Subsystem...");
        FRenderCommand::Init();
    }

    void FRenderer::Shutdown() {
        LE_CORE_INFO("Shutting down Renderer Subsystem...");
    }

    void FRenderer::OnWindowResize(unsigned int InWidth, unsigned int InHeight) {
        FRenderCommand::SetViewport(0, 0, InWidth, InHeight);
    }

    void FRenderer::BeginScene() {}

    void FRenderer::EndScene() {}

    void FRenderer::Submit(const TRef<FShader>& InShader, const TRef<FVertexArray>& InVertexArray,
                           unsigned int InVertexCount) {
        InShader->Bind();
        InVertexArray->Bind();
        FRenderCommand::DrawArrays(InVertexArray, InVertexCount);
    }

    void FRenderer::SubmitIndexed(const TRef<FShader>& InShader, const TRef<FVertexArray>& InVertexArray,
                                  unsigned int InIndexCount) {
        InShader->Bind();
        InVertexArray->Bind();
        FRenderCommand::DrawIndexed(InVertexArray, InIndexCount);
    }

} // namespace Leon
