#include "engine/renderer/Renderer.hpp"
#include "engine/core/Log.hpp"
#include <glm/gtc/type_ptr.hpp>

namespace Leon {

    TScope<FRenderer::FSceneData> FRenderer::s_SceneData = MakeScope<FRenderer::FSceneData>();
    FRenderStats FRenderer::s_Stats;
    static size_t s_SwapchainBytes = 0;

    void FRenderer::Init() {
        LE_CORE_INFO("Initializing Renderer Subsystem...");
        FRenderCommand::Init();

        // Default swapchain allocation accounting: Front Color (4) + Back Color (4) + Depth/Stencil (4) = 12 bytes/px
        s_SwapchainBytes = 1280 * 720 * 12;
        OnGPUAlloc(s_SwapchainBytes);
    }

    void FRenderer::Shutdown() {
        LE_CORE_INFO("Shutting down Renderer Subsystem...");
        OnGPUFree(s_SwapchainBytes);
        s_SwapchainBytes = 0;
    }

    void FRenderer::OnWindowResize(unsigned int InWidth, unsigned int InHeight) {
        FRenderCommand::SetViewport(0, 0, InWidth, InHeight);

        OnGPUFree(s_SwapchainBytes);
        s_SwapchainBytes = static_cast<size_t>(InWidth * InHeight * 12);
        OnGPUAlloc(s_SwapchainBytes);
    }

    void FRenderer::BeginScene(const FPerspectiveCamera& InCamera) {
        if (!s_SceneData)
            s_SceneData = MakeScope<FSceneData>();

        s_SceneData->ViewProjectionMatrix = InCamera.GetViewProjectionMatrix();
        s_SceneData->CameraPosition = InCamera.GetPosition();

        ResetStats();
    }

    void FRenderer::BeginScene() {
        ResetStats();
    }

    void FRenderer::EndScene() {}

    void FRenderer::Submit(const TRef<FShader>& InShader, const TRef<FVertexArray>& InVertexArray,
                           unsigned int InVertexCount) {
        InShader->Bind();
        if (s_SceneData) {
            InShader->SetMat4("u_ViewProjection", glm::value_ptr(s_SceneData->ViewProjectionMatrix));
            InShader->SetFloat3("u_ViewPos", s_SceneData->CameraPosition.x, s_SceneData->CameraPosition.y,
                                s_SceneData->CameraPosition.z);
        }
        InVertexArray->Bind();
        FRenderCommand::DrawArrays(InVertexArray, InVertexCount);

        s_Stats.DrawCalls++;
        s_Stats.VertexCount += InVertexCount;
        s_Stats.TriangleCount += InVertexCount / 3;
    }

    void FRenderer::SubmitIndexed(const TRef<FShader>& InShader, const TRef<FVertexArray>& InVertexArray,
                                  unsigned int InIndexCount) {
        InShader->Bind();
        if (s_SceneData) {
            InShader->SetMat4("u_ViewProjection", glm::value_ptr(s_SceneData->ViewProjectionMatrix));
            InShader->SetFloat3("u_ViewPos", s_SceneData->CameraPosition.x, s_SceneData->CameraPosition.y,
                                s_SceneData->CameraPosition.z);
        }
        InVertexArray->Bind();
        FRenderCommand::DrawIndexed(InVertexArray, InIndexCount);

        unsigned int count = InIndexCount
                                 ? InIndexCount
                                 : (InVertexArray->GetIndexBuffer() ? InVertexArray->GetIndexBuffer()->GetCount() : 0);
        s_Stats.DrawCalls++;
        s_Stats.IndexCount += count;
        s_Stats.TriangleCount += count / 3;
        s_Stats.VertexCount += count;
    }

    void FRenderer::SubmitLines(const TRef<FShader>& InShader, const TRef<FVertexArray>& InVertexArray,
                                unsigned int InVertexCount) {
        InShader->Bind();
        if (s_SceneData) {
            InShader->SetMat4("u_ViewProjection", glm::value_ptr(s_SceneData->ViewProjectionMatrix));
            InShader->SetFloat3("u_ViewPos", s_SceneData->CameraPosition.x, s_SceneData->CameraPosition.y,
                                s_SceneData->CameraPosition.z);
        }
        InVertexArray->Bind();
        FRenderCommand::DrawLines(InVertexArray, InVertexCount);

        s_Stats.DrawCalls++;
        s_Stats.VertexCount += InVertexCount;
    }

} // namespace Leon
