#include "engine/renderer/Renderer.hpp"
#include "engine/core/Log.hpp"
#include <glm/gtc/type_ptr.hpp>

namespace Leon {

    TScope<FRenderer::FSceneData> FRenderer::s_SceneData = MakeScope<FRenderer::FSceneData>();

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

    void FRenderer::BeginScene(const FPerspectiveCamera& InCamera) {
        if (!s_SceneData)
            s_SceneData = MakeScope<FSceneData>();

        s_SceneData->ViewProjectionMatrix = InCamera.GetViewProjectionMatrix();
        s_SceneData->CameraPosition = InCamera.GetPosition();
    }

    void FRenderer::BeginScene() {}

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
    }

} // namespace Leon
