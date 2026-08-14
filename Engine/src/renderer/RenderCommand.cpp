#include "renderer/RenderCommand.hpp"
#include "renderer/Renderer.hpp"
#include "renderer/VertexArray.hpp"

namespace Leon {

    TScope<IRenderAPI> FRenderCommand::s_RenderAPI = nullptr;

    void FRenderCommand::DrawArrays(const TRef<FVertexArray>& InVertexArray, unsigned int InVertexCount) {
        if (s_RenderAPI && InVertexArray) {
            s_RenderAPI->DrawArrays(InVertexArray, InVertexCount);
            FRenderer::RecordDrawArrays(InVertexCount);
        }
    }

    void FRenderCommand::DrawIndexed(const TRef<FVertexArray>& InVertexArray, unsigned int InIndexCount) {
        if (s_RenderAPI && InVertexArray) {
            unsigned int count = InIndexCount;
            if (count == 0 && InVertexArray->GetIndexBuffer()) {
                count = InVertexArray->GetIndexBuffer()->GetCount();
            }
            s_RenderAPI->DrawIndexed(InVertexArray, InIndexCount);
            FRenderer::RecordDrawIndexed(count, count);
        }
    }

    void FRenderCommand::DrawLines(const TRef<FVertexArray>& InVertexArray, unsigned int InVertexCount) {
        if (s_RenderAPI && InVertexArray) {
            s_RenderAPI->DrawLines(InVertexArray, InVertexCount);
            FRenderer::RecordDrawLines(InVertexCount);
        }
    }

} // namespace Leon
