#include "RHI/FRenderCommand.hpp"
#include "RHI/FRenderer.hpp"
#include "RHI/FVertexArray.hpp"

namespace Leon {

    TScope<IRenderAPI> FRenderCommand::RenderAPI = nullptr;

    void FRenderCommand::DrawArrays(const TRef<FVertexArray>& InVertexArray, unsigned int InVertexCount) {
        if (RenderAPI && InVertexArray) {
            RenderAPI->DrawArrays(InVertexArray, InVertexCount);
            FRenderer::RecordDrawArrays(InVertexCount);
        }
    }

    void FRenderCommand::DrawIndexed(const TRef<FVertexArray>& InVertexArray, unsigned int InIndexCount) {
        if (RenderAPI && InVertexArray) {
            unsigned int count = InIndexCount;
            if (count == 0 && InVertexArray->GetIndexBuffer()) {
                count = InVertexArray->GetIndexBuffer()->GetCount();
            }
            RenderAPI->DrawIndexed(InVertexArray, count);
            FRenderer::RecordDrawIndexed(count, count);
        }
    }

    void FRenderCommand::DrawIndexedOffset(const TRef<FVertexArray>& InVertexArray, unsigned int InIndexCount,
                                           unsigned int InIndexOffset) {
        if (RenderAPI && InVertexArray) {
            RenderAPI->DrawIndexedOffset(InVertexArray, InIndexCount, InIndexOffset);
            FRenderer::RecordDrawIndexed(InIndexCount, InIndexCount);
        }
    }

    void FRenderCommand::DrawLines(const TRef<FVertexArray>& InVertexArray, unsigned int InVertexCount) {
        if (RenderAPI && InVertexArray) {
            RenderAPI->DrawLines(InVertexArray, InVertexCount);
            FRenderer::RecordDrawLines(InVertexCount);
        }
    }

} // namespace Leon
