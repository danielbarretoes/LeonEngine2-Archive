#pragma once

namespace Leon {

    /**
     * @brief Real-time rendering diagnostic metrics and telemetry.
     */
    struct FRenderStats {
        unsigned int DrawCalls = 0;
        unsigned int IndexCount = 0;
        unsigned int VertexCount = 0;
        unsigned int TriangleCount = 0;
        size_t AllocatedGPUMemoryBytes = 0;

        void Reset() {
            DrawCalls = 0;
            IndexCount = 0;
            VertexCount = 0;
            TriangleCount = 0;
        }
    };

} // namespace Leon
