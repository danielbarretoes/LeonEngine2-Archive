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

    void FRenderCommand::DrawIndexedInstanced(const TRef<FVertexArray>& InVertexArray, unsigned int InIndexCount,
                                              unsigned int InInstanceCount) {
        if (RenderAPI && InVertexArray && InInstanceCount > 0) {
            unsigned int count = InIndexCount;
            if (count == 0 && InVertexArray->GetIndexBuffer()) {
                count = InVertexArray->GetIndexBuffer()->GetCount();
            }
            RenderAPI->DrawIndexedInstanced(InVertexArray, count, InInstanceCount);
            FRenderer::RecordDrawIndexed(count * InInstanceCount, count * InInstanceCount);
        }
    }

    void FRenderCommand::DrawIndexedOffsetInstanced(const TRef<FVertexArray>& InVertexArray, unsigned int InIndexCount,
                                                    unsigned int InIndexOffset, unsigned int InInstanceCount) {
        if (RenderAPI && InVertexArray && InInstanceCount > 0) {
            RenderAPI->DrawIndexedOffsetInstanced(InVertexArray, InIndexCount, InIndexOffset, InInstanceCount);
            FRenderer::RecordDrawIndexed(InIndexCount * InInstanceCount, InIndexCount * InInstanceCount);
        }
    }

    void FRenderCommand::DrawLines(const TRef<FVertexArray>& InVertexArray, unsigned int InVertexCount) {
        if (RenderAPI && InVertexArray) {
            RenderAPI->DrawLines(InVertexArray, InVertexCount);
            FRenderer::RecordDrawLines(InVertexCount);
        }
    }

} // namespace Leon
