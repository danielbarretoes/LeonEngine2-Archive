#pragma once

#include "engine/renderer/RenderCommand.hpp"
#include "engine/renderer/Shader.hpp"
#include "engine/renderer/VertexArray.hpp"

namespace Leon {

    class FRenderer {
    public:
        static void Init();
        static void Shutdown();

        static void OnWindowResize(unsigned int InWidth, unsigned int InHeight);

        static void BeginScene();
        static void EndScene();

        static void Submit(const TRef<FShader>& InShader, const TRef<FVertexArray>& InVertexArray,
                           unsigned int InVertexCount = 0);
        static void SubmitIndexed(const TRef<FShader>& InShader, const TRef<FVertexArray>& InVertexArray,
                                  unsigned int InIndexCount = 0);

        static ERenderAPI GetAPI() { return IRenderAPI::GetAPI(); }
    };

    using Renderer = FRenderer;

} // namespace Leon
