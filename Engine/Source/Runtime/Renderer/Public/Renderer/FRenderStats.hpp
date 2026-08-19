#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace Leon {

    enum class EGPUMemoryCategory : uint8_t {
        Texture2D = 0,
        TextureCube,
        Framebuffer,
        VertexBuffer,
        IndexBuffer,
        UniformBuffer,
        Swapchain,
        Count
    };

    struct FGPUMemoryBucket {
        size_t Bytes = 0;
        uint32_t Allocations = 0;
    };

    /**
     * @brief Real-time rendering diagnostic metrics and telemetry.
     */
    struct FRenderStats {
        unsigned int DrawCalls = 0;
        unsigned int IndexCount = 0;
        unsigned int VertexCount = 0;
        unsigned int TriangleCount = 0;
        unsigned int MeshesDrawn = 0;
        unsigned int MeshesCulled = 0;
        unsigned int ShaderChanges = 0;
        unsigned int TextureBinds = 0;
        unsigned int VAOBinds = 0;
        unsigned int FBOSwitches = 0;

        // Persistent across Reset() — GPU resources live longer than a frame.
        size_t AllocatedGPUMemoryBytes = 0;
        FGPUMemoryBucket GPUMemory[static_cast<size_t>(EGPUMemoryCategory::Count)]{};
        std::unordered_map<std::string, FGPUMemoryBucket> NamedGPUMemory;

        void Reset() {
            DrawCalls = 0;
            IndexCount = 0;
            VertexCount = 0;
            TriangleCount = 0;
            MeshesDrawn = 0;
            MeshesCulled = 0;
            ShaderChanges = 0;
            TextureBinds = 0;
            VAOBinds = 0;
            FBOSwitches = 0;
        }
    };

} // namespace Leon
