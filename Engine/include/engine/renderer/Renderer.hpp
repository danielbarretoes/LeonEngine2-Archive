#pragma once

#include "engine/renderer/PerspectiveCamera.hpp"
#include "engine/renderer/RenderCommand.hpp"
#include "engine/renderer/Shader.hpp"
#include "engine/renderer/VertexArray.hpp"

namespace Leon {

    class FRenderer {
    public:
        static void Init();
        static void Shutdown();

        static void OnWindowResize(unsigned int InWidth, unsigned int InHeight);

        static void BeginScene(const FPerspectiveCamera& InCamera);
        static void BeginScene();
        static void EndScene();

        static void Submit(const TRef<FShader>& InShader, const TRef<FVertexArray>& InVertexArray,
                           unsigned int InVertexCount = 0);
        static void SubmitIndexed(const TRef<FShader>& InShader, const TRef<FVertexArray>& InVertexArray,
                                  unsigned int InIndexCount = 0);

        static ERenderAPI GetAPI() { return IRenderAPI::GetAPI(); }

    private:
        struct FSceneData {
            glm::mat4 ViewProjectionMatrix = glm::mat4(1.0f);
            glm::vec3 CameraPosition = glm::vec3(0.0f);
        };

        static TScope<FSceneData> s_SceneData;
    };

    using Renderer = FRenderer;

} // namespace Leon
