#pragma once

#include "renderer/PerspectiveCamera.hpp"
#include "renderer/RenderCommand.hpp"
#include "renderer/Shader.hpp"
#include "renderer/VertexArray.hpp"

#include "renderer/RenderStats.hpp"

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
        static void SubmitLines(const TRef<FShader>& InShader, const TRef<FVertexArray>& InVertexArray,
                                unsigned int InVertexCount);

        static const FRenderStats& GetStats() { return s_Stats; }
        static void ResetStats() { s_Stats.Reset(); }

        static void OnGPUAlloc(size_t InBytes) { s_Stats.AllocatedGPUMemoryBytes += InBytes; }
        static void OnGPUFree(size_t InBytes) {
            if (s_Stats.AllocatedGPUMemoryBytes >= InBytes)
                s_Stats.AllocatedGPUMemoryBytes -= InBytes;
            else
                s_Stats.AllocatedGPUMemoryBytes = 0;
        }
        static size_t GetAllocatedGPUMemory() { return s_Stats.AllocatedGPUMemoryBytes; }

        static ERenderAPI GetAPI() { return IRenderAPI::GetAPI(); }

    private:
        struct FSceneData {
            glm::mat4 ViewProjectionMatrix = glm::mat4(1.0f);
            glm::vec3 CameraPosition = glm::vec3(0.0f);
        };

        static TScope<FSceneData> s_SceneData;
        static FRenderStats s_Stats;
    };

    using Renderer = FRenderer;

} // namespace Leon
