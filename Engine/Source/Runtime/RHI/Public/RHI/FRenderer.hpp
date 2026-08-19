#pragma once

#include "RHI/FRenderCommand.hpp"
#include "Renderer/FRenderStats.hpp"

namespace Leon {

    /**
     * @brief Centralized render stats tracker and GPU memory accounting.
     * Draw submission now lives in FWorldRenderer. This class only tracks metrics.
     */
    class FRenderer {
    public:
        static void Init();
        static void Shutdown();
        static void OnWindowResize(unsigned int InWidth, unsigned int InHeight);

        static const FRenderStats& GetStats() { return Stats; }
        static FRenderStats& GetStatsMutable() { return Stats; }
        static void ResetStats() { Stats.Reset(); }

        static void RecordDrawIndexed(unsigned int InIndexCount, unsigned int InVertexCount) {
            Stats.DrawCalls++;
            Stats.IndexCount += InIndexCount;
            Stats.TriangleCount += InIndexCount / 3;
            Stats.VertexCount += InVertexCount ? InVertexCount : InIndexCount;
        }

        static void RecordDrawArrays(unsigned int InVertexCount) {
            Stats.DrawCalls++;
            Stats.VertexCount += InVertexCount;
            Stats.TriangleCount += InVertexCount / 3;
        }

        static void RecordDrawLines(unsigned int InVertexCount) {
            Stats.DrawCalls++;
            Stats.VertexCount += InVertexCount;
        }

        static void OnGPUAlloc(size_t InBytes, EGPUMemoryCategory InCategory, const char* InLabel = nullptr);
        static void OnGPUFree(size_t InBytes, EGPUMemoryCategory InCategory, const char* InLabel = nullptr);
        static size_t GetAllocatedGPUMemory() { return Stats.AllocatedGPUMemoryBytes; }

        static ERenderAPI GetAPI() { return IRenderAPI::GetAPI(); }

    private:
        static FRenderStats Stats;
    };

} // namespace Leon
