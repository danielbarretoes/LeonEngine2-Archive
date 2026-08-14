#pragma once

#include "renderer/RenderCommand.hpp"
#include "renderer/RenderStats.hpp"

namespace Leon {

    /**
     * @brief Centralized render stats tracker and GPU memory accounting.
     * Draw submission now lives in FSceneRenderer. This class only tracks metrics.
     */
    class FRenderer {
    public:
        static void Init();
        static void Shutdown();
        static void OnWindowResize(unsigned int InWidth, unsigned int InHeight);

        static const FRenderStats& GetStats() { return s_Stats; }
        static void ResetStats() { s_Stats.Reset(); }

        static void RecordDrawIndexed(unsigned int InIndexCount, unsigned int InVertexCount) {
            s_Stats.DrawCalls++;
            s_Stats.IndexCount += InIndexCount;
            s_Stats.TriangleCount += InIndexCount / 3;
            s_Stats.VertexCount += InVertexCount ? InVertexCount : InIndexCount;
        }

        static void RecordDrawArrays(unsigned int InVertexCount) {
            s_Stats.DrawCalls++;
            s_Stats.VertexCount += InVertexCount;
            s_Stats.TriangleCount += InVertexCount / 3;
        }

        static void RecordDrawLines(unsigned int InVertexCount) {
            s_Stats.DrawCalls++;
            s_Stats.VertexCount += InVertexCount;
        }

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
        static FRenderStats s_Stats;
    };

} // namespace Leon
